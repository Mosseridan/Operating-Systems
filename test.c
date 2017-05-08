#include "types.h"
#include "user.h"
#include "stat.h"
#include "uthread.h"

#define N 100 // buffer size;

int access;
struct counting_semaphore* empty;
struct counting_semaphore* full;
int buffer[N];
int in = 0;
int out = 0;


void
produces(void* arg)
{
  for(int i = 1; i<1001; i++){
      down(empty);
      bsem_dwon(access);
      buffer[in] = i;
      printf(1, "producer %d produced %d and inserted it at buffer[%d].\n",uthread_self(),i,in);
      in = (in + 1) % N;
      bsem_up(access);
      up(full);
  }
}

void
consumer(void* arg) {
  int item;
  while(true){
    down(full);
    down(access);
    item = buffer[out];
    printf(1,"consumer %d consumed %d from buffer[%d].\n",uthread_self(),item,out);
    out = (out + 1) % N;
    up(access);
    up(empty);
    sleep(item);
    printf(1,"Thread %d slept for %d ticks.",uthread_self(),item);
  }
}




int
main(int argc, char *argv[]){

printf(1,"------------------TEXTING----------------- \n");


uthread_init();
access = bsem_alloc();
empty = csem_alloc(N);
full = csem_alloc(0);

for(int i = 0; i<3; i++){
  uthread_create(consumer,0);
}

produces(0);


printf(1,"-----------------main thread is exiting--------------- \n");
uthread_exit();
printf(1,"-----------------should not be here!!!--------------- \n");
}
