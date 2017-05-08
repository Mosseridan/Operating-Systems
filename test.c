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
  for(int i = 1; i<1004; i++){
      down(empty);
      bsem_down(access);
      bq_enqueue(bq,(void*)i);
      bsem_up(access);
      up(full);
  }
}

void
consumer(void* arg) {
  int item;
  while(1){
    down(full);
    bsem_down(access);
    item = (int)bq_dequeue(bq);
    if(item > 1000){
      bsem_up(access);
      up(empty);
      break;
    }
    bsem_up(access);
    up(empty);
    //printf(1,"Thread %d going to sleep for %d ticks.\n",uthread_self(),item);
    uthread_sleep(item);
    printf(1,"Thread %d slept for %d ticks.\n",uthread_self(),item);
  }
}


int
main(int argc, char *argv[]){

  int consumers[3];


  uthread_init();
  access = bsem_alloc();
  empty = csem_alloc(N);
  full = csem_alloc(0);
  bq = bq_alloc(N);

  for(int i = 0; i<3; i++){
    consumers[i] = uthread_create(consumer,0);
  }

  int prod = uthread_create(producer,0);
  uthread_join(prod);

  for(int i = 0; i<3; i++){
    uthread_join(consumers[i]);
  }

  bsem_free(access);
  csem_free(empty);
  csem_free(full);

  uthread_exit();
}
