#include "kernel/types.h"

#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int i;
  char *nargv[MAXARG];

  if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')) {
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  // 传入系统调用的参数是1个数字,执行trace系统调用
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  // 执行该命令
  for (i = 2; i < argc && i < MAXARG; i++) {
    nargv[i - 2] = argv[i];
  }
  // 使用exec取代该进程,也就是说将当前的进程替换成为后面的执行流
  exec(nargv[0], nargv);
  exit(0);
}
