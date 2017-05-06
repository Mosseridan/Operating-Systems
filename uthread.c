#include "uthread.h"
#include "user.h"
#include "param.h"

uint temp = 0;


uint nexttid = 1;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

void
uthread_schedule_wrapper(int signum){
  uint tf;
  asm("movl %%ebp, %0;" :"=r"(tf) : :);
  tf+=8;
  // tf+=12; //USE THIS WHEN PRINTING!!!
  // printf(1,"in wrapper %x\n", tf); //IF U ADD THIS LINE BACK U MUST REPLACE THE TF+8 to TF+12!!!
  uthread_schedule((struct trapframe*)tf);
}

int
uthread_init()
{
  struct uthread* ut = uttable;
  ut->tid = nexttid++;
  ut->pid = getpid();
  ut->state = RUNNING;
  ut->tstack = 0; //the main thread is using the regular user's stack, no need to free at uthread_exit
  current = ut;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  sigsend(ut->pid, SIGALRM);//in order to ensure that the trapframe of the main thread is backed up on the user's stack as a side effect of the signal handling.
  alarm(UTHREAD_QUANTA);
  // printf(1, "in uthread_init after alarm 5\n");
  return ut->tid;
}

int
uthread_create(void (*start_func)(void*), void* arg)
{
  alarm(0);//disabling SIGALARM interupts to make uthread_create an atomic method
  // printf(1, "in uthread_create after alarm 0\n");
  struct uthread *ut;
  uint sp;

  for(ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->state == UNUSED)
      goto found;
  }
  return -1;

  found:
    ut->tid = nexttid++;
    ut->pid = getpid();
    ut->tf = current->tf;

    // Allocate thread stack.
    if((ut->tstack = (uint)malloc(TSTACKSIZE)) == 0){//saving a pointer to the thread's stack
      ut->state = UNUSED;
      return -1;
    }

    sp = ut->tstack + TSTACKSIZE;
    // push arg
    sp -= 4;
    *(void**)sp = arg;
    // push return address to thread_exit
    sp -= 4;
    *(void**)sp = uthread_exit;
    // initialize thread stack pointers
    ut->tf.esp = sp;
    // set threads eip to start_func
    ut->tf.eip = (uint)start_func;
    //printf(1,"!! current->tf.eip,"current->tf-<)
    ut->state = RUNNABLE;
    alarm(UTHREAD_QUANTA);//allowing SIGALARM to interupt again
    return ut->tid;
}

void
uthread_schedule(struct trapframe* tf)
{
  alarm(0);
  struct uthread *ut = current;
  // back up the tf already on the stack to the current running thread's tf only if the current thread is not dead yet
  if(ut->state == RUNNING){
    memmove((void*)&ut->tf, tf, sizeof(struct trapframe));
    ut->state = RUNNABLE;
  }
  ut++;
  while(ut->state != RUNNABLE && ut->tid != current->tid){
   ut++;
   if(ut >= &uttable[MAX_UTHREADS])
     ut = uttable;
  }
  // copy the tf of the thread to be run next on to currnt user stack so we will rever back to it at sigreturn;
  memmove(tf, (void*)&ut->tf, sizeof(struct trapframe));
  if(current->state == UNUSED && current->tstack)
   free((void*)current->tstack);
  current = ut;
  if(ut->state != RUNNABLE){
    if(ut->tstack)
     free((void*)ut->tstack);
    exit();
  }
  ut->state = RUNNING;
  alarm(UTHREAD_QUANTA);
  return;
}

void
uthread_exit()
{
  alarm(0);
  struct uthread *ut = current;
  ut->state = ZOMBIE;
  ut++;
  while(ut->state != RUNNABLE && ut->state == SLEEPING && ut->state != ZOMBIE){
   ut++;
   if(ut >= &uttable[MAX_UTHREADS])
     ut = uttable;
  }

  if(ut->state == ZOMBIE){
    if(current->tstack)
      free((void*)current->tstack);
    exit();
  }
  current->state = UNUSED;
  sigsend(current->pid, SIGALRM);
  return;
}

int
uthread_self()
{
  return current->tid;
}

int uthread_join(int tid)
{
  return 0;
}

int uthread_sleep(int ticks)
{
  return 0;
}
