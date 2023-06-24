#include "types.h"

#include "riscv.h"

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "spinlock.h"

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
  if (n < 0)
    n = 0;
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
  backtrace();
  return 0;
}

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

// 应该将alarm interval和the pointer to the handler function存储到pro字段中
uint64 sys_sigalarm(void) {
  int interval = 0;
  argint(0, &interval);
  if (interval < 0) {
    return -1;
  }
  uint64 func = 0;
  argaddr(1, &func);
  struct proc *p = myproc();
  p->interval = interval;
  p->func = func;
  return 0;
}

// 恢复寄存器
uint64 sys_sigreturn(void) {
  struct proc *p = myproc();
  p->save.over = 0; // 已经完成本次操作
  p->trapframe->epc = p->save.epc;
  p->trapframe->ra = p->save.ra;
  p->trapframe->sp = p->save.sp;
  p->trapframe->gp = p->save.gp;
  p->trapframe->tp = p->save.tp;
  p->trapframe->t0 = p->save.t0;
  p->trapframe->t1 = p->save.t1;
  p->trapframe->t2 = p->save.t2;
  p->trapframe->s0 = p->save.s0;
  p->trapframe->s1 = p->save.s1;
  p->trapframe->a1 = p->save.a1;
  p->trapframe->a2 = p->save.a2;
  p->trapframe->a3 = p->save.a3;
  p->trapframe->a4 = p->save.a4;
  p->trapframe->a5 = p->save.a5;
  p->trapframe->a6 = p->save.a6;
  p->trapframe->a7 = p->save.a7;
  p->trapframe->s2 = p->save.s2;
  p->trapframe->s3 = p->save.s3;
  p->trapframe->s4 = p->save.s4;
  p->trapframe->s5 = p->save.s5;
  p->trapframe->s6 = p->save.s6;
  p->trapframe->s7 = p->save.s7;
  p->trapframe->s8 = p->save.s8;
  p->trapframe->s9 = p->save.s9;
  p->trapframe->s10 = p->save.s10;
  p->trapframe->s10 = p->save.s11;
  p->trapframe->t3 = p->save.t3;
  p->trapframe->t4 = p->save.t4;
  p->trapframe->t5 = p->save.t5;
  p->trapframe->t6 = p->save.t6;
  // 设置a0在别的地方
  return 0;
}
