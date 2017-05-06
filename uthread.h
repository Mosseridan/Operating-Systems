#include "types.h"
#include "x86.h"
#include "param.h"


enum tstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, JOINNING };

struct uthread {
  uint tid;   //thread id
  uint tstack; //pointer to the thread's stack
  uint uttable_index;
  enum tstate state; //thread state (UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  struct trapframe tf; //the trapframe backed up on the user stack
  uint pid; //the pid of the process running this thread
  struct uthread* joined_on_me[MAX_UTHREADS-1]; //an array of pointers to uthreads that are joined on uthread
  int wakeup; //the time for the thread to wake-up if sleeping
};

//uthread.cei
int uthread_init(void);
int uthread_create(void (*) (void*), void*);
void uthread_schedule(struct trapframe*);
void uthread_exit(void);
int uthread_self(void);
int uthread_join(int);
int uthread_sleep(int);
