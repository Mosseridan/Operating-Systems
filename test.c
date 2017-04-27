#include "types.h"
#include "user.h"
#include "stat.h"
typedef void (*sighandler_t)(int);

void
test(int sigNum){
 printf(1,"\n=======================Signal Handler===================================\n Process id:  %d  Signal number: %d \n\n", getpid(),sigNum);
}


int
main(int argc, char *argv[]){

int j;
printf(1,"------------------TestEx1----------------- \n");
sighandler_t handler=(sighandler_t)test;
for(int i=0;i<32;i++){
 printf(1,"test  for1: i=%d\n",i);
 signal(i,handler);
}

for(int j=0;j<32;j++){
 sigsend(getpid(),j);
}
for(j=0; j<15; j++)
	  sleep(1);
exit();
}
