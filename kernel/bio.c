// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"

#include "param.h"
#include "spinlock.h"

#include "riscv.h"
#include "sleeplock.h"

#include "defs.h"
#include "fs.h"

#include "buf.h"

#define BUCKET_SIZE 13

static char *s[13] = {
    "bcache.bucket0",  "bcache.bucket1", "bcache.bucket2",  "bcache.bucket3",
    "bcache.bucket4",  "bcache.bucket5", "bcache.bucket6",  "bcache.bucket7",
    "bcache.bucket8",  "bcache.bucket9", "bcache.bucket10", "bcache.bucket11",
    "bcache.bucket12",
};

// 实验要求修改bget和brelse函数当并发lookups和release的时候不依赖于bcache.lock锁
// 在搜索缓冲区,找不到的时候为其分配缓冲区操作是原子的
// 在bget中选择refcnt==0的缓冲区进行删除,release并不需要获取全局锁
// 在替换块的时候可能将一个桶中的buf放到另一个桶中
struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // 每一个桶中含有一个head
  struct buf head[BUCKET_SIZE];
  struct spinlock bucket[BUCKET_SIZE];
} bcache;

void binit(void) {
  struct buf *b;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    initlock(&bcache.bucket[i], s[i]);
    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  // 每一个平均到相应的桶中
  int i = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.head[i].next;
    b->prev = &bcache.head[i];
    initsleeplock(&b->lock, "buffer");
    bcache.head[i].next->prev = b;
    bcache.head[i].next = b;
    i = ((++i) % BUCKET_SIZE) == 0 ? 0 : i;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  uint pos = blockno % BUCKET_SIZE;

  // 查看当前的blockno是否在相关的桶中,当前并不需要获取全局的锁
  acquire(&bcache.bucket[pos]);
  // Is the block already cached?
  for (b = bcache.head[pos].next; b != &bcache.head[pos]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket[pos]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 先查看当前的桶中是否有refcnt == 0的进行换出
  // 代码执行到这的时候依旧含有pos的桶锁,先查找pos的桶中是否含有refcnt==0的buf
  // 如果有则进行设置值,以及释放pos的锁
  for (b = bcache.head[pos].prev; b != &bcache.head[pos]; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bucket[pos]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 代码执行到此处依旧含有pos的bucket锁
  // 先获取其余的锁,查看其余的bucket是否含有refcnt==0的情况
  // 如果存在则进行删除和添加操作,之后在进行释放两个桶中的锁
  // 如果当前桶中不存在,那么释放当前的桶中的锁
  for (int i = 0; i < BUCKET_SIZE; i++) {
    if (i == pos) {
      continue;
    }
    acquire(&bcache.bucket[i]); // 其余
    for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        acquiresleep(&b->lock);
        break;
      }
    }
    if (b != &bcache.head[i]) { // 已经找到了一个buf
      // 需要将这个buf在当前桶i中进行删除,然后放到桶pos的地方
      // delete
      b->next->prev = b->prev;
      b->prev->next = b->next;
      // add head
      b->next = bcache.head[pos].next;
      b->prev = &bcache.head[pos];
      bcache.head[pos].next->prev = b;
      bcache.head[pos].next = b;
      release(&bcache.bucket[i]);
      release(&bcache.bucket[pos]);
      return b;
    } else {
      release(&bcache.bucket[i]);
    }
  }
  release(&bcache.bucket[pos]);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  // 修改数据获取当前的桶锁
  int pos = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket[pos]);
  b->refcnt--;
  release(&bcache.bucket[pos]);
}

void bpin(struct buf *b) {
  uint pos = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket[pos]);
  b->refcnt++;
  release(&bcache.bucket[pos]);
}

void bunpin(struct buf *b) {
  uint pos = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket[pos]);
  b->refcnt--;
  release(&bcache.bucket[pos]);
}
