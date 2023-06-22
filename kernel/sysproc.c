#include "types.h"

#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"

#include "defs.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64 sys_sbrk(void) {
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

#ifdef LAB_PGTBL
// 获取当前的页表的访问权限
int sys_pgaccess(void) {
  uint64 addr = 0;
  argaddr(0, &addr);
  int size = 0;
  argint(1, &size);
  if (size > 64) {
    panic("size is too long");
  }
  uint64 mask = 0;
  argaddr(2, &mask);
  // 根据相应的用户addr虚拟地址,来访问对应的物理地址页表的访问(R/W)
  uint64 ans = 0;

  struct proc *pro = myproc();
  acquire(&pro->lock);
  for (int i = 0; i < size; i++) {
    pte_t *pte = walk(myproc()->pagetable, addr + i * PGSIZE, 0);
    if ((*pte & PTE_A) == PTE_A || (*pte & PTE_D) == PTE_D) {
      // 当前页面已经被访问过,修改ans的标识位
      *pte &= ~(PTE_A | PTE_D); // 清除标记位(修改全局变量需要上锁)
      ans |= (1 << i);
    }
  }
  release(&pro->lock);
  if (copyout(myproc()->pagetable, mask, (char *)&ans, sizeof(ans)) < 0) {
    return -1;
  }
  return 0;
}
#endif

uint64 sys_kill(void) {
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
