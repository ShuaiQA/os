#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <kernel/param.h>

void xargs(char *buf, int argc, char *argv[]) {
  char *p = buf;
  char *pre = buf;
  // 查找换行符
  while (*p != '\0') {
    if (*p == '\n') {
      char last_argv[80];
      memmove(last_argv, pre, p - pre);
      last_argv[p - pre + 1] = '\0';
      char *next[MAXARG];
      for (int i = 0; i < argc - 1; i++) {
        next[i] = argv[i + 1];
      }
      next[argc - 1] = last_argv;
      int pid = fork();
      if (pid == -1) {
        exit(1);
      }
      if (pid == 0) {
        exec(next[0], next);
      } else {
        wait(0);
      }
      p++;
      pre = p;
    } else {
      p++;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "usage: xargs [file ...]\n");
    exit(1);
  }
  char buf[1024]; // 暂时处理上一个输入是1024个字符的
  int size;
  int pos = 0;
  while ((size = read(0, buf + pos, 1)) > 0) {
    pos += size;
    if (pos >= 1024) {
      fprintf(2, "buf size is small");
      exit(1);
    }
  }
  xargs(buf, argc, argv);
  exit(0);
}
