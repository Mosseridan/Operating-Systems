#include "types.h"
#include "user.h"
#include "stat.h"
#include "uthread.h"

uint TEMP;

void
test(int sigNum){
 printf(1,"\n=======================Signal Handler===================================\n Process id:  %d  Signal number: %d \n\n", getpid(),sigNum);
}

void
foo(void* arg)
{
  if(uthread_self()%3==0)
    uthread_join(uthread_self()-1);
  for(int i= 10; i>0; i--){
    // printf(1,"tid: %d, i: %d\n",uthread_self(),i);
    // uthread_sleep(13);
    uthread_sleep(13);
  }
  printf(1,"tid: %d is ending\n",uthread_self());
}

void
foo2(void* arg)
{
  // if(uthread_self()%5==0)
    uthread_join(uthread_self()-1);
  // printf(1,"tid: %d\n",uthread_self());
  for(int i= 10; i>0; i--){
    printf(1,"tid: %d, i: %d\n",uthread_self(),i);
  }
  printf(1,"tid: %d is ending\n",uthread_self());
}

void
foo3(void* arg)
{

  for(int i = 0; i<80; i++){
      printf(1,"#%d:%d#",uthread_self(),i);
      uthread_sleep(10);
  }
  printf(1,"&&&%d done\n",uthread_self());
}


int
main(int argc, char *argv[]){

printf(1,"------------------TestEx1----------------- \n");
// int a = 100;
int b = 100;
//int c = 20;
//uint temp;
//asm("call next1; next1: popl %%eax; call next2; next2: popl %%ebx;subl %%ebx,%%eax; movl %%eax, %0;" :"=r"(temp) : :"%eax","%ebx");
// asm("movl %%esp, %0;" :"=r"(temp) : :);
// printf(1,"in main: temp: %x\n",temp);
// asm("pushl $123;call next; next: popl %eax; addl $12, %eax; pushl %eax;jmp foo;");
// asm("movl %%esp, %0;" :"=r"(temp) : :);
// printf(1,"in main: temp: %x\n",temp);
uthread_init();
for(int i=0;i<64;i++){
  uthread_create(foo3,&b);
}
uthread_join(63);
uthread_sleep(1000);
printf(1,"-----------------main thread is exiting--------------- \n");

uthread_exit();

printf(1,"-----------------should not be here!!!--------------- \n");


// for(int i=0; i < 100; i++){
//   if(i%2 == 0)
//     uthread_create(foo,&b);
//   else
//     uthread_create(foo2,&b);
//   uthread_sleep(5);
// }
//foo(&c);
//for(;;){}
// for(int i = 0; i < 10; i++){
//   sleep(1);
// }
// uthread_exit();
}
