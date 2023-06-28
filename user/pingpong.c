// parent send ping
#include "kernel/types.h"

#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int pipe_fd[2];  // 父写子读
  int pipe_fd1[2]; // 子写父读
  int pid;
  // 创建管道
  if (pipe(pipe_fd) == -1 || pipe(pipe_fd1) == -1) {
    exit(1);
  }

  // 创建子进程
  pid = fork();
  if (pid == -1) {
    exit(1);
  }

  if (pid == 0) { // 子进程
    close(pipe_fd[1]);
    close(pipe_fd1[0]);

    char buffer[128];
    int pd = getpid();
    while (1) {
      int read_bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
      if (read_bytes <= 0) {
        break;
      }
      buffer[read_bytes] = '\0';
      printf("%d: received %s", pd, buffer);
    }
    close(pipe_fd[0]);

    const char *message = "pong\n";
    write(pipe_fd1[1], message, strlen(message));
    close(pipe_fd1[1]);
  } else { // 父进程
    close(pipe_fd[0]);
    close(pipe_fd1[1]);

    int pd = getpid();
    const char *message = "ping\n";
    write(pipe_fd[1], message, strlen(message));
    close(pipe_fd[1]);

    char buffer[128];
    while (1) {
      int read_bytes = read(pipe_fd1[0], buffer, sizeof(buffer) - 1);
      if (read_bytes <= 0) {
        break;
      }
      buffer[read_bytes] = '\0';
      printf("%d: received %s", pd, buffer);
    }
    close(pipe_fd1[0]);
  }

  exit(0);
}
