#include "types.h"
#include "stat.h"
#include "user.h"

void
default_sig_handler(int signum){
  int pid =0;
  // simulate calling getpid(void) by pushing 0 which is argc,
  // putting 18 which is SYS_getpid in eax and declaring int 64 (interupt handler)
  // moving the result to pid
  asm("pushl $0; movl $18, %%eax; int $64; movl %%eax, %0;"
      :"=r"(pid) /* output to pid */
      : /* no input registers */
      :"%eax" /* clobbered register */
    );
  printf(1,"A signal %d was accepted by %d\n",signum,pid);
}

int
main(int argc, char *argv[])
{
  default_sig_handler(123);
  return 0;
}
