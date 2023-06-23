//
// formatted console output -- printf, panic.
//

#include "param.h"
#include "types.h"
#include <stdarg.h>

#include "spinlock.h"

#include "fs.h"
#include "sleeplock.h"

#include "file.h"
#include "memlayout.h"
#include "riscv.h"

#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign) {
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}

static void printptr(uint64 x) {
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(char *fmt, ...) {
  va_list ap;
  int i, c, locking;
  char *s;

  locking = pr.locking;
  if (locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c) {
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }
  va_end(ap);

  if (locking)
    release(&pr.lock);
}

void panic(char *s) {
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for (;;)
    ;
}

void printfinit(void) {
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}

// 为了支持回溯,编译器会产生机器代码维持`栈帧`,反映了函数调用链
// 每一个栈帧是由一个返回地址(ret)和指向调用栈帧的帧指针
// 寄存器s0保存的是一个指针值(该值是当前的栈帧的地址),ret地址+8的位置
// struct frame{
//    uint64 ret_addr;      // 当前ret的值是在高地址(栈是从高向低进行增长的)
//    frame *next_frame;    // 指向下一个栈帧,当前的地址是在ret地址-8
// }
// frame *first_frame = read_s0_reg
// 当前的s0寄存器保存的第一个栈帧的地址(该值其实是指向的是ret_addr地址的末尾最后的地址)
// 所以需要addr-8获取ret,以及addr-16获取下一个frame
void backtrace() {
  uint64 fp = r_fp();
  printf("backtrace:\n", fp);
  while (PGROUNDUP(fp) - PGROUNDDOWN(fp) == PGSIZE) {
    uint64 ret = *(uint64 *)(fp - 8);
    printf("%p\n", ret);
    fp = *(uint64 *)(fp - 16);
  }
}
