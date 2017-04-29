enum tstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct uthread {
  uint tid;   //thread id
  char* tstack; //thread stack pointer
  uint tip; //thread instruction pointer
  enum tstate state; //thread state (UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  struct trapframe* tf; //a pointer to the trapframe backed up on the user stack
  uint pid; //the pid of the process running this thread
};

//uthread.c
int uthread_init(void);
int uthread_create(void (*) (void*), void*);
void uthread_schedule(void);
void uthread_exit(void);
int uthread_self(void);
int uthread_join(int);
int uthread_sleep(int);