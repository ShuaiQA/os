// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "param.h"
#include "types.h"

#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"

#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

char *s[NCPU] = {"kmem0", "kmem1", "kmem2", "kmem3",
                 "kmem4", "kmem5", "kmem6", "kmem7"};

// 该函数是由hartid=0进行调用的,需要将kmem数组中的freelist均匀分配
void kinit() {
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, s[i]);
  }
  freerange(end, (void *)PHYSTOP);
}

// 平均分析初始化每一个CPU的物理页表,由于当前函数是由CPU0进行初始化的
// 并且是唯一一个进程,个人认为不需要添加锁
void avg(int pos, void *pa) {
  struct run *r;
  memset(pa, 1, PGSIZE);
  r = (struct run *)pa;
  r->next = kmem[pos].freelist;
  kmem[pos].freelist = r;
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  int i = 0;
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    avg(i, p);
    i = (++i) == NCPU ? 0 : i;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  // 对应相关的CPU的list
  int cid = cpuid();
  acquire(&kmem[cid].lock);
  r->next = kmem[cid].freelist;
  kmem[cid].freelist = r;
  release(&kmem[cid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 最开始我的操作是将cid全部替换为cpuid()函数,这样会通不过测试,思考为什么？
// 经过测试后(查看注释的那一部分)发现临界区的代码一共有两个部分,一个是当前的CPU分配
// 另一个是其余的CPU分配,但是在后面一部分的代码中,可以涉及到相关的中断的发生
// 导致当前的代码发生调度执行在其余的CPU上面,导致下面的if判断发生错误
void *kalloc(void) {
  struct run *r;

  int cid = cpuid();
  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if (r)
    kmem[cid].freelist = r->next;
  release(&kmem[cid].lock);

  // 如果当前的CPU分配的页表不够了,需要从其余的CPU页表中进行获取
  if (r == 0) {
    for (int i = 0; i < NCPU; i++) {
      // if (cid != cpuid()) {
      //   panic("error");
      // }
      if (i == cid) { // 当前已经没有页面了
        continue;
      }
      acquire(&kmem[i].lock);
      if (kmem[i].freelist != 0) {
        r = kmem[i].freelist;
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk

  return (void *)r;
}
