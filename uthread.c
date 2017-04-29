#include "types.h"
#include "user.h"
#include "x86.h"
#include "uthread.h"
#include "param.h"


struct uthread uttable[MAX_UTHREADS];
int nexttid = 1;

void
uthread_schedule_wrapper(int signum){
  uthread_schedule();//TODO: WTF?
}

static struct uthread*
allocuthread(void)
{
  struct uthread *ut;
  char *sp;

  for(int i=0; i < MAX_UTHREADS; i++){
    ut = &uttable[i];
    if(ut->state == UNUSED)
      goto found;
  }
  return 0;

  found:
    ut->state = EMBRYO;
    ut->tid = nexttid++;
    ut->pid = getpid();
    signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
    alarm(UTHREAD_QUANTA);

    // Allocate kernel stack.
    if((ut->tstack = malloc(TSTACKSIZE)) == 0){
      ut->state = UNUSED;
      return 0;
    }
    sp = ut->tstack + TSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *ut->tf;
    ut->tf = (struct trapframe*)sp;

    return ut;
}

//TODO: Set EIP to be something we don't know just yet
int
uthread_init()
{
  struct uthread* nt = allocuthread();
  nt->state = RUNNABLE;
  return 0;
}

int uthread_create(void (*start_func) (void*), void* arg)
{
  return 0;
}

void uthread_schedule()
{

}

void uthread_exit()
{

}

int uthread_self()
{
  return 0;
}

int uthread_join(int tid)
{
  return 0;
}

int uthread_sleep(int ticks)
{
  return 0;
}
