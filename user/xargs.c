#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  char c;
  char buf[512];
  char *buf_argv[MAXARG];
  if (argc < 2) {
    printf("xargs: missing command");
  }
  for(int i=1; i < argc; i++)
    buf_argv[i-1] = argv[i];

  // read one byte at a time, util it reach the \n or 0
  while(read(0, &c, 1) && c != 0) {
    memset(buf, 0, MAXARG);
    int i = 0;
    buf[i++] = c;
    while(read(0, &c, 1) && c != 0 && c != '\n') {
      buf[i++] = c;
    }
    buf[i] = 0;
    buf_argv[argc - 1] = buf;
    buf_argv[argc] = 0;
    
    if (fork() == 0) {
      exec(buf_argv[0], buf_argv);
      fprintf(2, "exec %s failed\n", argv[1]);
      exit(1);
    }
    wait(0);
  }
  exit(0);
}