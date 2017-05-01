#include "types.h"
#include "user.h"
#include "stat.h"
#include "uthread.h"
typedef void (*sighandler_t)(int);

void
test(int sigNum){
 printf(1,"\n=======================Signal Handler===================================\n Process id:  %d  Signal number: %d \n\n", getpid(),sigNum);
}


int
main(int argc, char *argv[]){

printf(1,"------------------TestEx1----------------- \n");
uthread_init();
for(int i = 0; i <10000;i++){
  sleep(1);
  //printf(1,"tid: %d, i: %d\n",uthread_self(),i);
}
exit();
}
