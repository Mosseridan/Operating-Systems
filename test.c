#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

void fib(int n) {
	if (n <= 1)
		return;
	fib(n-1);
	fib(n-2);
}


int
main(int argc, char *argv[])
{
  int i, pid;
  for(i=0; i<15; i++){
    pid=fork();
      if(i==10 || i==0 || i==14){
        if(pid==0){
          printf(1,"\n\nstarting lsof:\n");
          exec("lsof", argv);
          exit();
        } else{
          wait();
        }
      }else if(pid==0){
          fib(39);
          exit();
      }
  }

  for(i=0; i<12; i++)
    wait();
	exit();
}
