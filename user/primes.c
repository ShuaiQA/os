#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

// 当前的子进程执行的逻辑是，取出管道中的数据，输出第一个数据
// 然后筛选出该倍数的数据进行写到下一个管道中
// 结束的条件是当前的进程管道中没有数据
void process(int pipeid[2]) {
  close(pipeid[1]);
  int data = 0;
  if (read(pipeid[0], &data, 4) <= 0) {
    // 首先可以肯定的是如果存在进程向该管道写入数据那么read操作会阻塞
    // 除非会close该管道才会返回小于等于0，对应结束的条件
    close(pipeid[0]); // 注意没有读出数据需要关闭
    return;
  }
  printf("prime %d\n", data);
  // read读取该数据到data中，创建另一个管道1，从当前进程会从该管道中进行读取数据写到管道1中
  int pipe1[2];
  if (pipe(pipe1) == -1) {
    exit(1);
  }
  int pid = fork();
  if (pid == -1) {
    exit(1);
  }
  if (pid == 0) {
    close(pipeid[0]); // **
    process(pipe1);
  } else {
    close(pipe1[0]);
    while (1) {
      int next;
      int read_bytes = read(pipeid[0], &next, 4);
      if (read_bytes <= 0) {
        break;
      }
      if (next % data != 0) {
        write(pipe1[1], &next, 4);
      }
    }
    close(pipe1[1]);
    close(pipeid[0]);
    wait(0);
  }
}

int main() {
  int pipeid[2];
  if (pipe(pipeid) == -1) {
    exit(1);
  }
  int pid = fork();
  if (pid == -1) {
    exit(1);
  }
  if (pid == 0) {
    process(pipeid);
  } else { // 父进程向管道中添加数据
    close(pipeid[0]);
    for (int i = 2; i < 35; i++) {
      write(pipeid[1], &i, 4);
    }
    close(pipeid[1]);
    wait(0);
  }
  return 0;
}
