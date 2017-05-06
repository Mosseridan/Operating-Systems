#include "uthread.h"
#include "user.h"

uint nexttid = 1;
int living_threads = 0;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

void
uthread_schedule_wrapper(int signum){
  uint tf;
  asm("movl %%ebp, %0;" :"=r"(tf) : :);
  tf+=8;
  // tf+=4; //ADD THIS WHEN PRINTING!!! (TOTAL SHOULD BE TF + 12)
  // printf(1,"in wrapper %x\n", tf); //IF U ADD THIS LINE BACK U MUST REPLACE THE TF+8 to TF+12!!!
  uthread_schedule((struct trapframe*)tf);
}

int
uthread_init()
{
  struct uthread* ut = &uttable[0];
  ut->tid = nexttid++;
  ut->pid = getpid();
  ut->state = RUNNING;
  ut->tstack = 0; //the main thread is using the regular user's stack, no need to free at uthread_exit
  ut->uttable_index = 0;
  living_threads++;
  current = ut;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  sigsend(ut->pid, SIGALRM);//in order to ensure that the trapframe of the main thread is backed up on the user's stack as a side effect of the signal handling.
  // alarm(UTHREAD_QUANTA);//TODO: I think we don't need this because of the sigsend: We go straight to uthread_sched and ther we already have alarm(UTHREAD_QUANTA)
  return ut->tid;
}

int
uthread_create(void (*start_func)(void*), void* arg)
{
  alarm(0);//disabling SIGALARM interupts to make uthread_create an atomic method
  living_threads++;
  struct uthread *ut;
  uint sp;

  for(ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->state == UNUSED)
      goto found;
  }
  printf(1,"@@@@@@@@@@@@@NO ROOM FOR MORE THREADS!!!");
  return -1;

  found:
    ut->tid = nexttid++;
    ut->pid = getpid();
    ut->tf = current->tf;
    ut->uttable_index = ut-uttable;

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
    ut->state = RUNNABLE;
    alarm(UTHREAD_QUANTA);//allowing SIGALARM to interupt again
    return ut->tid;
}

void
uthread_schedule(struct trapframe* tf)
{
  alarm(0);//disabling alarms to prevent synchronization problems
  struct uthread *ut = current;
  // back up the tf already on the stack to the current running thread's tf only if the current thread is not dead yet
  // printf(1, "in uthread_schedule: current->tid: %d  current->state: %d\n", current->tid, current->state);
  memmove((void*)&ut->tf, tf, sizeof(struct trapframe));
  if(ut->state == RUNNING){
    // printf(1, "%d: changing this to runnable in sched\n",ut->tid);
    ut->state = RUNNABLE;
    // printf(1, "changed %d's  state to: %d\n", current->tid, current->state);
  }

  ut++;
  if(ut >= &uttable[MAX_UTHREADS])
    ut = uttable;
  while(ut->state != RUNNABLE){
   if(ut->state == SLEEPING){
    //  printf(1, "%d: this thread is waiting on a wakeup call\n",ut->tid);
     if(ut->wakeup > 0 && ut->wakeup <= uptime()){
      ut->wakeup = 0;
      // printf(1, "%d: changing this to runnable in wakeup\n",ut->tid);
      ut->state = RUNNABLE;
      break;
     }
   }
   ut++;
   if(ut >= &uttable[MAX_UTHREADS])
     ut = uttable;
  }
  // printf(1, "%d: this is the thread the sched chose to run. state: %d\n",ut->tid, ut->state);
  // copy the tf of the thread to be run next on to currnt user stack so we will rever back to it at sigreturn;
  memmove(tf, (void*)&ut->tf, sizeof(struct trapframe));

  if(current->state == UNUSED && current->tstack)
   free((void*)current->tstack);

  current = ut;

  if(ut->state != RUNNABLE && living_threads){
    printf(1, "freeing stack\n");//TODO: MAKE SURE THIS IS WORKING!!!
    if(ut->tstack)
     free((void*)ut->tstack);
    exit();
  }
  if(ut->state == RUNNABLE){
    ut->state = RUNNING;
    alarm(UTHREAD_QUANTA);//allowing alarms again
  } else
    sigsend(current->pid, SIGALRM);
  return;
}

void
uthread_exit()
{
  alarm(0);//disabling alarms to prevent synchronization problems
  // printf(1,"now closing %d\n", current->tid);
  living_threads--;
  current->state = ZOMBIE;
  for(int i=0; i<MAX_UTHREADS-1; i++){
    if(current->joined_on_me[i]){
      // printf(1, "%d: changing this to runnable in exit when closing %d\n",current->joined_on_me[i]->state, current->tid);
      current->joined_on_me[i]->state = RUNNABLE;
      current->joined_on_me[i] = 0;
    }
  }

  if(living_threads == 0){
    if(current->tstack)
      free((void*)current->tstack);
    exit();
  }
  current->state = UNUSED;
  sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
  return;
}

int
uthread_self()
{
  return current->tid;
}

int uthread_join(int tid)
{
  alarm(0);//disabling alarms to prevent synchronization problems
  // printf(1, "%d is trying to join %d\n", current->tid, tid);
  if(tid > 0 && tid < nexttid && tid != current->tid ){//if an illegal tid is entered do nothing
    // printf(1, "tid: %d is legal\n", tid);
    for(int i=0; i<MAX_UTHREADS; i++){
      if((uttable[i].tid == tid) && (uttable[i].state == RUNNABLE || uttable[i].state == SLEEPING || uttable[i].state == JOINNING)){//if there is an existing thread with this TID
        // printf(1, "i: %d found %d and he is %d\n", i, tid, uttable[i].state);
        current->state = JOINNING;
        uttable[i].joined_on_me[current->uttable_index] = current;
        sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
        return 0;
      }
    }
  }
  // printf(1, "could not find %d\n", tid);
  // alarm(UTHREAD_QUANTA);//allowing alarms again
  sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
  return -1;//illegal tid or no runnable thread with such tid
}

int uthread_sleep(int ticks)
{
  alarm(0);
  int current_ticks = uptime();
  if(ticks <= 0){
    // alarm(UTHREAD_QUANTA);
    sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
    return -1;
  }
  current->state = SLEEPING;
  current->wakeup = ticks+current_ticks;
  sigsend(current->pid, SIGALRM);
  return 0;
}
