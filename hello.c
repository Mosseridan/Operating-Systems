#include "types.h"
#include "stat.h"
#include "user.h"

void
fib(int n)
{
	if (n <= 1)
		return;
	fib(n-1);
	fib(n-2);
}

int
main(int argc, char *argv[])
{
  int childpid[5], status[5];
  printf(1,"Hello World!\n");
  for(int i = 0; i < 5; i++){
    if((childpid[i] = fork()) == 0){
        printf(1,"child process %d strting fib\n",getpid());
        exec("bin/fib", argv);
        exit(0);
    }
    else{
      printf(1,"parent process %d forked child %d\n",getpid(), childpid[i]);
    }
  }
  for(int j = 0; j<5 ; j++){
    wait(&(status[j]));
  }

  return (0);
}
