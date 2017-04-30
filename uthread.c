#include "types.h"
#include "user.h"
#include "x86.h"
#include "uthread.h"
#include "param.h"



void
uthread_schedule_wrapper(int signum){
  uthread_schedule();//TODO: WTF?
}

static struct uthread*
allocuthread(void)
{
  struct uthread *ut;
  char *sp;

  for(int i=0; i < MAX_UTHREADS; i++){  // TODO: change this to :(ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++)
    ut = &uttable[i];
    if(ut->state == UNUSED)
      goto found;
  }
  return 0;

  found:
    ut->state = EMBRYO;
    ut->tid = nexttid++;
    ut->pid = getpid();

    // Allocate thred stack.
    if((ut->tstack = malloc(TSTACKSIZE)) == 0){
      ut->state = UNUSED;
      return 0;
    }
    sp = ut->tstack + TSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *ut->tf;
    ut->tf = (struct trapframe*)sp;

    // initialize thread stack pointers
    ut->tf->ebp = sp;
    ut->tf->esp = sp;

    return ut;
}

//TODO: Set EIP to be something we don't know just yet
int
uthread_init()
{

  struct uttable* uttable = malloc(sizeof(struct uttable));
  if(!uttable)
    return -1;

  struct uthread* nt = allocuthread();
  if(!nt)
    return -1;

  uttable->current = nt;

  nt->state = RUNNABLE;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  alarm(UTHREAD_QUANTA);
  return nt->tid;
}

int uthread_create(void (*start_func)(void*), void* arg)
{
  struct uthread* nt = allocuthread();
  if(!nt)
    return -1;

  void* sp = nt->tf->esp;
  // push arg
  sp -= 4;
  *sp = arg;
  // push argc
  sp -= 4;
  *sp = 1;
  // push return address to thread_exit
  sp -= 4;
  *sp = uthread_exit;
  // reset threads esp in tf
  nt->tf->esp = sp;
  // set threads eip to start_func
  nt->tf->eip = start_func;
  //nt->eip = start_func; TODO: remove?

  return nt->tid;
}

void uthread_schedule()
{
  struct uthread *ut;




  for(;;){

    // Loop over uthreads table looking for uthreads to run.
    for(ut = uttable->currnt; ut < &uttable[MAX_UTHREADS]; ut++){
      if(ut->state != RUNNABLE)
        continue;

      // Switch to chosen uthread.
      uthread = ut;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
  }
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
