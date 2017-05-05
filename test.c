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
  //printf(1,"in foo: arg: %x\n",arg);
  for(int i= 10; i>0; i--){
    printf(1,"tid: %d, i: %d\n",uthread_self(),i);
    sleep(13);
  }
  // for(;;);
}

void
foo2(void* arg)
{
  for(int i= 10; i>0; i--){
    printf(1,"tid: %d, i: %d\n",uthread_self(),i);
  }
// for(;;);
}


// void fuck(){
//   asm("pop %%eax; movl %%eax, %0;" :"=r"(TEMP) : :"eax");
//   printf(1,"\n!!!FOO: %x\n",TEMP);
// }


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
// uthread_create(foo,&a);
uthread_create(foo,&b);
uthread_create(foo2,&b);
uthread_create(foo,&b);
uthread_create(foo2,&b);
uthread_create(foo,&b);
//foo(&c);
for(;;){}
exit();
}
