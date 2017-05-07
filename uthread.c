#include "uthread.h"
#include "user.h"

uint nexttid = 1;
int living_threads = 0;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

struct bsem* bsemtable[MAX_BSEM];

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
  // living_threads++; //why don't I need this?
  current = ut;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  sigsend(ut->pid, SIGALRM);//in order to ensure that the trapframe of the main thread is backed up on the user's stack as a side effect of the signal handling.
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
    printf(1,"@!! creating %d at uttable[%d] !!@\n",ut->tid,ut->uttable_index);

    // Allocate thread stack if no yet allocated.
    if(!ut->tstack){
      printf(1,"!!! allocating stack for %d at uttable[%d] !!!\n",ut->tid,ut->uttable_index);
      if((ut->tstack = (uint)malloc(TSTACKSIZE)) == 0){//saving a pointer to the thread's stack
        ut->state = UNUSED;
        sigsend(current->pid, SIGALRM);
        return -1;
      }
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
    // printf(1, ".");
    if(ut->state == SLEEPING && ut->wakeup > 0 && ut->wakeup <= uptime()){
       //  printf(1, "%d: this thread is waiting on a wakeup call\n",ut->tid);
      ut->wakeup = 0;
      // printf(1, "%d: changing this to runnable in wakeup\n",ut->tid);
      ut->state = RUNNABLE;
    // } else if(ut->state == SLEEPING){
        // printf(1,"%d is still waiting for a wakeup call: %d %d", ut->tid, ut->wakeup, uptime());
    } else if(ut->state == UNUSED && ut->tstack && ut->tid != current->tid && ut->tid != 1){
      // printf(1,"@$$@freeing stack for %d at uttable[%d]\n",ut->tid,ut-uttable);
      free((void*)ut->tstack);
      ut->tstack = 0;
    }
    else if(ut->state == UNUSED && ut->tstack && ut->tid != current->tid && ut->tid != 1){
      printf(1,"@$$@freeing stack for %d at uttable[%d]\n",ut->tid,ut-uttable);
      free((void*)ut->tstack);
      ut->tstack = 0;
    }
    ut++;
    if(ut >= &uttable[MAX_UTHREADS])
      ut = uttable;
  }

  // printf(1, "%d: this is the thread the sched chose to run. state: %d\n",ut->tid, ut->state);
  // copy the tf of the thread to be run next on to currnt user stack so we will rever back to it at sigreturn;
  memmove(tf, (void*)&ut->tf, sizeof(struct trapframe));

  // if(current->state == UNUSED && current->tstack)
  //  free((void*)current->tstack);
  current = ut;
  ut->state = RUNNING;
  //printf(1, "in uthread_schedule: switching %d:%d -> %d:%d\n", current->tid, current->state, ut->tid,ut->state);
  alarm(UTHREAD_QUANTA);
  return;
  // if(ut->state != RUNNABLE && living_threads){
  //   printf(1, "freeing stack\n");//TODO: MAKE SURE THIS IS WORKING!!!
  //   if(ut->tstack)
  //    free((void*)ut->tstack);
  //   exit();
  // }
  // if(ut->state == RUNNABLE){
  //   ut->state = RUNNING;
  //   alarm(UTHREAD_QUANTA);//allowing alarms again
  // } else
  //   sigsend(current->pid, SIGALRM);
  // return;

}

void
uthread_exit()
{

  alarm(0);//disabling alarms to prevent synchronization problems
  // printf(1,"now closing %d\n", current->tid);
  living_threads--;
  //current->state = ZOMBIE;

  // for(int i=0; i<MAX_UTHREADS-1; i++){
  //   if(current->joined_on_me[i]){
  //     // printf(1, "%d: changing this to runnable in exit when closing %d\n",current->joined_on_me[i]->state, current->tid);
  //     current->joined_on_me[i]->state = RUNNABLE;
  //     current->joined_on_me[i] = 0;
  //   }
  // }
  for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->state == SLEEPING && ut->joining == current->tid){
      ut->joining = 0;
      ut->state = RUNNABLE;
      // printf(1,"%d woke up %d\n",current->tid,ut->tid);
    }
  }

  // printf(1,"%d $$$is exiting and living_threads are now %d\n",current->tid,living_threads);
  // int temp = living_threads;
  // printf(1,"%d $$$is exiting and temp is now %d\n",current->tid,temp);

  if(living_threads < 1){
    // printf(1,"%d last thread exiting....\n",current->tid);
    for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
      if(ut->tstack){
        // printf(1,"@@@ freeing stack for %d at uttable[%d] @@@\n",ut->tid,ut->uttable_index);
        free((void*)ut->tstack);
        ut->tstack = 0;
      }
    }
    exit();
  }

  // printf(1,"%d @@@is exiting and living_threads are now %d\n",current->tid,living_threads);

  current->state = UNUSED;
  sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
  return;
}

int
uthread_self()
{
  return current->tid;
}

int
uthread_join(int tid)
{
  // alarm(0);//disabling alarms to prevent synchronization problems
  // // printf(1, "%d is trying to join %d\n", current->tid, tid);
  // if(tid > 0 && tid < nexttid && tid != current->tid ){//if an illegal tid is entered do nothing
  //   // printf(1, "tid: %d is legal\n", tid);
  //   for(int i=0; i<MAX_UTHREADS; i++){
  //     if((uttable[i].tid == tid) && (uttable[i].state == RUNNABLE || uttable[i].state == SLEEPING || uttable[i].state == JOINNING)){//if there is an existing thread with this TID
  //       // printf(1, "i: %d found %d and he is %d\n", i, tid, uttable[i].state);
  //       current->state = JOINNING;
  //       uttable[i].joined_on_me[current->uttable_index] = current;
  //       sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
  //       return 0;
  //     }
  //   }
  // }
  // // printf(1, "could not find %d\n", tid);
  // // alarm(UTHREAD_QUANTA);//allowing alarms again
  // sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
  // return -1;//illegal tid or no runnable thread with such tid

  alarm(0);
  // printf(1, "%d is trying to join %d\n", current->tid, tid);
  for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->tid == tid && ut->state != UNUSED){
      current->joining = tid;
      current->state = SLEEPING;
      printf(1, "%d has joined %d\n", current->tid, tid);
      sigsend(current->pid, SIGALRM);
      return 0;
    }
  }
  printf(1, "%d has NOT joined %d no such tid\n", current->tid, tid);
  alarm(UTHREAD_QUANTA); //allowing alarms again
  return -1;
}

int uthread_sleep(int ticks)
{
//  printf(1, "%d is goint to sleep for %d ticks\n", current->tid, ticks);
  alarm(0);
  uint current_ticks = uptime();
  if(ticks < 0){
    sigsend(current->pid, SIGALRM);//instead of allowing alarms we send the signal and go to schedule where alarms will be allowed again
    return -1;
  }
  else if(ticks == 0){
    current->wakeup = 0;
  }
  else{
    current->wakeup = ticks+current_ticks;
  }
  current->state = SLEEPING;
  sigsend(current->pid, SIGALRM);
  return 0;
}

int
bsem_alloc()
{
  for(int sem = 0; sem < MAX_BSEM; sem++){
    if(!bsemtable[sem] && ((bsemtable[sem] = malloc(sizeof(struct bsem))) > 0)){
      bsemtable[sem]->s = 1;
      return sem;
    }
  }
  return -1;
}

void
bsem_free(int sem)
{
  if(bsemtable[sem]){
    free(bsemtable[sem]);
    bsemtable[sem] = 0;
  }
}

void
bsem_down(int sem)
{
  alarm(0);
  if(bsemtable[sem]->s){
    bsemtable[sem]->s = 0;
    alarm(UTHREAD_QUANTA);
  }
  else{
    bsemtable[sem]->waiting[current->uttable_index] = current;
    uthread_sleep(0);
  }
}

void bsem_up(int sem)
{
  alarm(0);
  for(struct uthread* ut = bsemtable[sem]->waiting[0]; ut < bsemtable[sem]->waiting[MAX_UTHREADS]; ut++){
    if(ut){ // there is a thread waiting on this semaphore -> wake him up
      ut->wakeup = uptime();
      alarm(UTHREAD_QUANTA);
      return;
    }
  }
  // there are no waiting threads
  bsemtable[sem]->s = 1;
  alarm(UTHREAD_QUANTA);
  return;
}

struct counting_semaphore*
alloc_csem(int init_val)
{
  struct counting_semaphore *sem;
  if((sem = malloc(sizeof(struct counting_semaphore))) == 0)
    return 0;

  sem->s1 = bsem_alloc();
  sem->s2 = bsem_alloc();
  if(init_val < 1){
    bsem_down(sem->s2);
  }
  sem->val = init_val;
  return sem;
}

void free_csem(struct counting_semaphore* sem)
{
  bsem_free(sem->s1);
  bsem_free(sem->s2);
  free(sem);
}
void
down(struct counting_semaphore* sem){
  bsem_down(sem->s2);
  bsem_down(sem->s1);
  sem->val--;
  if(sem->val > 0)
    bsem_up(sem->s2);
  bsem_up(sem->s1);
}

void
up(struct counting_semaphore* sem)
{
  bsem_down(sem->s1);
  sem->val++;
  if(sem->val ==1)
    bsem_up(sem->s2);
  bsem_up(sem->s1);
}
