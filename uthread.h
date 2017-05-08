#include "types.h"
#include "x86.h"
#include "param.h"


enum tstate { UNUSED, SLEEPING, RUNNABLE, RUNNING};//, ZOMBIE, JOINNING };

struct uthread {
  uint tid;   //thread id
  uint tstack; //pointer to the thread's stack
  enum tstate state; //thread state (UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  struct trapframe tf; //the trapframe backed up on the user stack
  uint pid; //the pid of the process running this thread
  uint joining;
  int wakeup; //the time for the thread to wake-up if sleeping
};

struct bound_queue{
  int in;
  int out;
  int contains;
  int size;
  void** queue;
};


struct bsem{
  uint s;
  struct bound_queue* waiting;
};

struct counting_semaphore{
  int val;
  int s1;
  int s2;
};

//uthread.c
int uthread_init(void);
int uthread_create(void (*) (void*), void*);
void uthread_schedule(struct trapframe*);
void uthread_exit(void);
int uthread_self(void);
int uthread_join(int);
int uthread_sleep(int);

struct bound_queue* bq_alloc(int);
void bq_free(struct bound_queue*);
void bq_enqueue(struct bound_queue*, void*);
void* bq_dequeue(struct bound_queue*);


int bsem_alloc(void);
void bsem_free(int);
void bsem_down(int);
void bsem_up(int);
struct counting_semaphore* csem_alloc(int init_val);
void csem_free(struct counting_semaphore*);
void down(struct counting_semaphore*);
void up(struct counting_semaphore*);
