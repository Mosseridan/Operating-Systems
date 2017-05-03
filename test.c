#include "types.h"
#include "user.h"
#include "stat.h"
// #include "x86.h"
#include "uthread.h"
typedef void (*sighandler_t)(int);

void
test(int sigNum){
 printf(1,"\n=======================Signal Handler===================================\n Process id:  %d  Signal number: %d \n\n", getpid(),sigNum);
}

void
foo(void* arg)
{
  for(int i=*(int*)arg; i>0; i--){
    printf(1,"tid: %d, i: %d\n",uthread_self(),i);
    sleep(1);
  }
}

int
main(int argc, char *argv[]){

printf(1,"------------------TestEx1----------------- \n");
// int a = 100;
int b = 23;
int c = 20;
uthread_init();
// uthread_create(foo,&a);
uthread_create(foo,&b);

foo(&c);

exit();
}
