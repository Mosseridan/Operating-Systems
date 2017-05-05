#include "types.h"
#include "x86.h"

enum tstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct uthread {
  uint tid;   //thread id
  uint tstack; //pointer to the thread's stack
  enum tstate state; //thread state (UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  struct trapframe tf; //the trapframe backed up on the user stack
  uint pid; //the pid of the process running this thread
};

//uthread.cei
int uthread_init(void);
int uthread_create(void (*) (void*), void*);
void uthread_schedule(struct trapframe*);
void uthread_exit(void);
int uthread_self(void);
int uthread_join(int);
int uthread_sleep(int);
