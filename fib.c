#include "types.h"
#include "user.h"

int
fib(int n)
{
	if (n <= 1) return n;
	return(fib(n-1) + fib(n-2));
}

int
main(int argc, char *argv[])
{
  int res;
  printf(1,"pid %d: calculating fib(38)...\n",getpid());
  res = fib(38);
  printf(1,"pid %d: fib(38) = %d\n",getpid(), res);
  // if(argc != 1){
  //   res = fib(38);
  //   printf(1,"fib(38) = %d\n",res);
  // }
  // else{
  //   res = fib(33);
  //   printf(1,"fib(33) = %d\n", argv[1],res);
  // }
	return 0;
}
