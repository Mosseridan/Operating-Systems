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
  living_threads++;
  current = ut;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  sigsend(ut->pid, SIGALRM);//in order to ensure that the trapframe of the main thread is backed up on the user's stack as a side effect of the signal handling.
  return ut->tid;
}

int
uthread_create(void (*start_func)(void*), void* arg)
{
  alarm(0);//disabling SIGALARM interupts to make uthread_create an atomic method
  struct uthread *ut;
  uint sp;

  for(ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->state == UNUSED)
      goto found;
  }
  printf(1,"!!!NO ROOM FOR MORE THREADS!!!\n");
  return -1;

  found:
    living_threads++;
    ut->tid = nexttid++;
    ut->pid = getpid();
    ut->tf = current->tf;

    // Allocate thread stack if no yet allocated.
    if(!ut->tstack){
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
  memmove((void*)&ut->tf, tf, sizeof(struct trapframe));
  if(ut->state == RUNNING){
    ut->state = RUNNABLE;
  }
  ut++;
  if(ut >= &uttable[MAX_UTHREADS])
    ut = uttable;
  while(ut->state != RUNNABLE){
    if(ut->state == SLEEPING && ut->wakeup > 0 && ut->wakeup <= uptime()){
      ut->wakeup = 0;
      ut->state = RUNNABLE;
      break;
    } else if(ut->state == UNUSED && ut->tstack && ut->tid != current->tid && ut->tid != 1){
      free((void*)ut->tstack);
      ut->tstack = 0;
    }
    ut++;
    if(ut >= &uttable[MAX_UTHREADS])
      ut = uttable;
  }

  // copy the tf of the thread to be run next on to currnt user stack so we will rever back to it at sigreturn;
  memmove(tf, (void*)&ut->tf, sizeof(struct trapframe));

  current = ut;
  ut->state = RUNNING;
  alarm(UTHREAD_QUANTA);
  return;
}

void
uthread_exit()
{
  alarm(0);//disabling alarms to prevent synchronization problems

  if(--living_threads <= 0){
    for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
      if(ut->tstack){
        free((void*)ut->tstack);
        ut->tstack = 0;
      }
    }
    exit();
  }

  for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->state == SLEEPING && ut->joining == current->tid){
      ut->joining = 0;
      ut->state = RUNNABLE;
    }
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

int
uthread_join(int tid)
{
  alarm(0);
  for(struct uthread* ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
    if(ut->tid == tid && ut->state != UNUSED){
      current->joining = tid;
      current->state = SLEEPING;
      sigsend(current->pid, SIGALRM);
      return 0;
    }
  }
  alarm(UTHREAD_QUANTA); //allowing alarms again
  return -1;
}

int uthread_sleep(int ticks)
{
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


struct bound_queue*
bq_alloc(int size){
  struct bound_queue* bq;
  if((bq = malloc(sizeof(struct bound_queue))) == 0)
    return 0;
  bq->in = 0;
  bq->out = 0;
  bq->size = size;
  if((bq->queue = malloc(sizeof(void*)*size)) == 0)
    return 0;
  return bq;
}

void
bq_free(struct bound_queue* bq){
  if(bq && bq->queue){
    free(bq->queue);
    free(bq);
  }
}

int
bq_enqueue(struct bound_queue* bq, void* item)
{
  int place = bq->in;
  bq->queue[place] = item;
  bq->in = (bq->in + 1) % bq->size;
  return place;
}

void*
bq_dequeue(struct bound_queue* bq)
{
  if(bq->in == bq->out)
    return 0;
  void* item = bq->queue[bq->out];
  bq->out= (bq->out + 1) % bq->size;
  return item;
}

int
bsem_alloc()
{
  for(int sem = 0; sem < MAX_BSEM; sem++){
    if(!bsemtable[sem] && ((bsemtable[sem] = malloc(sizeof(struct bsem))) > 0)){
        bsemtable[sem]->waiting = bq_alloc(MAX_UTHREADS);
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
    bq_free(bsemtable[sem]->waiting);
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
    bq_enqueue(bsemtable[sem]->waiting,current);
    uthread_sleep(0);
  }
}

void bsem_up(int sem)
{
  struct uthread* ut;
  alarm(0);
  ut = (struct uthread*)bq_dequeue(bsemtable[sem]->waiting);
  if(ut){
    ut->state = RUNNABLE;
    alarm(UTHREAD_QUANTA);
  }
  else{
    bsemtable[sem]->s = 1;
    alarm(UTHREAD_QUANTA);
  }
}

struct counting_semaphore*
csem_alloc(int init_val)
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

void csem_free(struct counting_semaphore* sem)
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
