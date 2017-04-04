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
	for(int i=1; i<50; i++){
			fib(25);
			sleep(1);
	}
	return 0;
}
