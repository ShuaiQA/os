// Mutual exclusion spin locks.

#include "types.h"

#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"

#include "defs.h"
#include "proc.h"

#ifdef LAB_LOCK
#define NLOCK 500

static struct spinlock *locks[NLOCK];
struct spinlock lock_locks;

void freelock(struct spinlock *lk) {
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if (locks[i] == lk) {
      locks[i] = 0;
      break;
    }
  }
  release(&lock_locks);
}

static void findslot(struct spinlock *lk) {
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if (locks[i] == 0) {
      locks[i] = lk;
      release(&lock_locks);
      return;
    }
  }
  panic("findslot");
}
#endif

void initlock(struct spinlock *lk, char *name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
#ifdef LAB_LOCK
  lk->nts = 0;
  lk->n = 0;
  findslot(lk);
#endif
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// 为什么上锁的过程之前需要关闭中断,以当前的kalloc进行分析
// 当一个用户进程需要页面的时候需要获取页面锁,如果在获取锁的临界区不关闭中断
// 会发生当当前的进程申请页面获取锁之后发生了中断
// 然后切换到另一个进程该进程也需要申请页面(注意页面的申请代码是在内核状态下)
// 这个时候由于上一个进程正在获取页面锁,所以内核代码不会执行下去在分配页面了
void acquire(struct spinlock *lk) {
  push_off();      // disable interrupts to avoid deadlock.
  if (holding(lk)) // 已经上锁了并且上锁的还是当前的CPU,抛出异常
    panic("acquire");

#ifdef LAB_LOCK
  __sync_fetch_and_add(&(lk->n), 1);
#endif

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
#ifdef LAB_LOCK
    __sync_fetch_and_add(&(lk->nts), 1);
#else
    ;
#endif
  }

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void release(struct spinlock *lk) {
  if (!holding(lk)) {
    printf("%d  %d\n", lk->locked, lk->cpu == mycpu());
    panic("release ");
  }

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int holding(struct spinlock *lk) {
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void push_off(void) {
  int old = intr_get();

  intr_off();
  if (mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1; // 代表push_off的嵌套层数
}

void pop_off(void) {
  struct cpu *c = mycpu();
  if (intr_get())
    panic("pop_off - interruptible");
  if (c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if (c->noff == 0 && c->intena)
    intr_on();
}

// Read a shared 32-bit value without holding a lock
int atomic_read4(int *addr) {
  uint32 val;
  __atomic_load(addr, &val, __ATOMIC_SEQ_CST);
  return val;
}

#ifdef LAB_LOCK
int snprint_lock(char *buf, int sz, struct spinlock *lk) {
  int n = 0;
  if (lk->n > 0) {
    n = snprintf(buf, sz, "lock: %s: #test-and-set %d #acquire() %d\n",
                 lk->name, lk->nts, lk->n);
  }
  return n;
}

int statslock(char *buf, int sz) {
  int n;
  int tot = 0;

  acquire(&lock_locks);
  n = snprintf(buf, sz, "--- lock kmem/bcache stats\n");
  for (int i = 0; i < NLOCK; i++) {
    if (locks[i] == 0)
      break;
    if (strncmp(locks[i]->name, "bcache", strlen("bcache")) == 0 ||
        strncmp(locks[i]->name, "kmem", strlen("kmem")) == 0) {
      tot += locks[i]->nts;
      n += snprint_lock(buf + n, sz - n, locks[i]);
    }
  }

  n += snprintf(buf + n, sz - n, "--- top 5 contended locks:\n");
  int last = 100000000;
  // stupid way to compute top 5 contended locks
  for (int t = 0; t < 5; t++) {
    int top = 0;
    for (int i = 0; i < NLOCK; i++) {
      if (locks[i] == 0)
        break;
      if (locks[i]->nts > locks[top]->nts && locks[i]->nts < last) {
        top = i;
      }
    }
    n += snprint_lock(buf + n, sz - n, locks[top]);
    last = locks[top]->nts;
  }
  n += snprintf(buf + n, sz - n, "tot= %d\n", tot);
  release(&lock_locks);
  return n;
}
#endif
