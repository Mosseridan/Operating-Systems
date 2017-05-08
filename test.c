#include "types.h"
#include "user.h"
#include "stat.h"
#include "uthread.h"

#define N 100 // buffer size;

int access;
struct counting_semaphore* empty;
struct counting_semaphore* full;
struct bound_queue* bq;

void
producer(void* arg)
{
  for(int i = 1; i<1001; i++){
      down(empty);
      bsem_down(access);
      // printf(1,"#########in producer with tid %d down access SUCESS\n",uthread_self());
      bq_enqueue(bq,(void*)i);
      printf(1, "producer %d produced %d.\n",uthread_self(),i);
      bsem_up(access);
      // printf(1,"#########in producer with tid %d up access SUCESS\n",uthread_self());
      up(full);
  }
}

void
consumer(void* arg) {
  int item;
  while(1){
    printf(1,"@@@@@@@@@@in consumer with tid %d\n",uthread_self());
    down(full);
    printf(1,"@@@@@@@@@@in consumer with tid %d dwon full SUCCESS\n",uthread_self());
    bsem_down(access);
    printf(1,"@@@@@@@@@@in consumer with tid %d dwon access SUCCESS\n",uthread_self());
    item = (int)bq_dequeue(bq);
    printf(1,"consumer %d consumed %d.\n",uthread_self(),item);
    bsem_up(access);
    printf(1,"@@@@@@@@@@in consumer with tid %d up access SUCCESS\n",uthread_self());
    up(empty);
    printf(1,"@@@@@@@@@@in consumer with tid %d up full SUCCESS\n",uthread_self());
    sleep(item);
    printf(1,"Thread %d slept for %d ticks.",uthread_self(),item);
  }
}




int
main(int argc, char *argv[]){

int consumers[3];
printf(1,"------------------TEXTING----------------- \n");


uthread_init();
access = bsem_alloc();
empty = csem_alloc(N);
full = csem_alloc(0);
bq = bq_alloc(N);

for(int i = 0; i<3; i++){
  consumers[i] = uthread_create(consumer,0);
  printf(1,"!!!!!!!!!!!!created consumer %d with tid %d\n",i,consumers[i]);
}

uthread_join(uthread_create(producer,0));

//producer(0);



for(int i = 0; i<3; i++){
  uthread_join(consumers[i]);
}

bsem_free(access);
csem_free(empty);
csem_free(full);

printf(1,"-----------------main thread is exiting--------------- \n");
uthread_exit();
printf(1,"-----------------should not be here!!!--------------- \n");
}
