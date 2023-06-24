// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"

#include "memlayout.h"
#include "param.h"
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
  uint num[PHYSTOP / PGSIZE]; // 索引下标是物理地址/PGSIZE
  struct spinlock lock;       // 修改num数组需要上锁
} knum;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit() {
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    knum.num[(uint64)p / PGSIZE] = 1;
    kfree(p);
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

  acquire(&kmem.lock);
  acquire(&knum.lock);
  // 计数器等于0的时候才需要进行释放当前的页面
  if (--knum.num[(uint64)pa / PGSIZE] == 0) {
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&knum.lock);
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  // 设置相应的页面为1
  acquire(&knum.lock);
  knum.num[(uint64)r / PGSIZE] = 1;
  release(&knum.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// 传入的参数不需要4096字节对齐
void addknum(void *pa) {
  acquire(&knum.lock);
  knum.num[(uint64)pa / PGSIZE]++;
  release(&knum.lock);
}

int getknum(void *pa) {
  int num;
  acquire(&knum.lock);
  num = knum.num[(uint64)pa / PGSIZE];
  release(&knum.lock);
  return num;
}
