#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

// 从后向前找到最后的path的name
char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void find(char *path, char *name, char *buf, char *p) {
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  strcpy(buf, path);
  p = buf + strlen(buf);

  switch (st.type) {
  case T_DEVICE:
  case T_FILE:
    if (strncmp(fmtname(path), name, strlen(name)) == 0) { //
      printf("%s\n", buf);
    }
    break;
  case T_DIR:
    // (每一次读取目录中的大小是dirent结构体中的数据获取文件的名字)
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if (strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0) {
        continue;
      }
      memmove(p, de.name, DIRSIZ); // 将目录中的内容放到buf中指针p
      p[DIRSIZ] = 0;
      find(buf, name, buf, p);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "usage: find [file ...]\n");
    exit(1);
  }
  char buf[512];
  char *p = buf;

  find(argv[1], argv[2], buf, p);
  exit(0);
}
