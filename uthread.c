#include "uthread.h"
#include "user.h"
#include "param.h"

uint temp = 0;


uint nexttid = 1;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

void
uthread_schedule_wrapper(int signum){
  uthread_schedule();//TODO: WTF?
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
  return ut->tid;
}

int
uthread_create(void (*start_func)(void*), void* arg)
{
  alarm(0);//disabling SIGALARM interupts to make uthread_create an atomic method
  //printf(1, "in uthread_create\n");
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
    if((ut->tstack = (uint)malloc(TSTACKSIZE)) == 0){
      ut->state = UNUSED;
      return -1;
    }

    sp = ut->tstack + TSTACKSIZE; //saving a pointer to the thread's stack

    // push arg
    sp -= 4;
    *(void**)sp = arg;
    //printf(1,"in uthread_create 2: current->tid: %d,  ut->tid: %d, sp: %x\n",current->tid, ut->tid, sp);
    // push argc
    // sp -= 4;
    // *(int*)sp = 1;
    // push return address to thread_exit
    sp -= 4;
    *(void**)sp = uthread_exit;
    //printf(1,"in uthread_create 3: current->tid: %d, ut->tid: %d, sp: %x\n",current->tid,ut->tid,sp);
    // initialize thread stack pointers
    //ut->ebp = sp;
    //ut->tf.ebp = sp;
    //ut->esp = sp;
    ut->tf.esp = sp;

    // set threads eip to start_func
    ut->tf.eip = (uint)start_func;
    //printf(1,"!! current->tf.eip,"current->tf-<)
    ut->state = RUNNABLE;
    alarm(UTHREAD_QUANTA);//allowing SIGALARM to interupt again
    return ut->tid;
}

void
uthread_schedule()
{
  // printf(1, "in uthread_schedule\n");

  uint tf;
  struct uthread *ut = current;

  // backup current thrad state by backing up its trap frame
  // asm("movl %%esp, %0;" :"=r"(tf) : :);
  // printf(1, "in uthread_schedule esp: %x\n", tf);

  asm("movl %%ebp, %0;" :"=r"(tf) : :);
  tf+=8;
  ut->tf = *((struct trapframe*)tf);
  ut->state = RUNNABLE;

  ut++;
  while(ut->state != RUNNABLE){
     ut++;
     if(ut >= &uttable[MAX_UTHREADS])
       ut = uttable;
  }

   //
  //  temp = ut->tf.ebp;
  //  asm("movl %1, %%ebp;" :"=r"(temp) :"r"(temp) :);
  //  temp = ut->ebp;
  //  asm("movl %1, %%ebp;" :"=r"(temp) :"r"(temp) :);
  //  temp = ut->esp;
  //  asm("movl %1, %%esp;" :"=r"(temp) :"r"(temp) :);

   //printf(1,"2in uthread_schedule temp: %x\n",temp);

   //
   // asm("movl %%esp, %0;" :"=r"(temp) : :);
   // printf(1,"3in uthread_schedule temp: %x\n",temp);

   // copy the tf of the thread to be run next on to currnt user stack so we will rever back to it at sigreturn;
   memmove((void*)tf, (void*)&ut->tf, sizeof(struct trapframe));
   current = ut;
   ut->state = RUNNING;
   alarm(UTHREAD_QUANTA);

   return;
}

void //TODO: remove this
temp123() {
  printf(1,"I'm here.. \n");
}

void
uthread_exit()
{
  // for(;;);
  // exit();
  alarm(0);
  printf(1, "in uthread_exit closing tid: %d\n",current->tid);
  struct uthread *ut = current;

  ut++;
  while(ut->state != RUNNABLE){
   ut++;
   if(ut >= &uttable[MAX_UTHREADS])
     ut = uttable;
  }

  printf(1,"!!!!!!!!!\n");
  if(current->tstack){
    free((void*)current->tstack);
  }
  printf(1,"?????????\n");

  if(ut->tid == current->tid){
    exit();
  }

  current->state = UNUSED;

  current = ut;
  current->state = RUNNING;
  printf(1, "in uthread_exit starting tid: %d\n",current->tid);
  uint bp  = current->tf.ebp;
  asm("movl %1, %%ebp;" :"=r"(bp) :"r"(bp) :);

  uint sp  = current->tf.esp;
  sp -= 4;
  *(void**)sp = temp123;// (uint)current->tf.eip;
  // memmove((void*)sp, (void*)temp, 4);
  printf(1, "in uthread_exit sp: %x temp123: %x\n",sp,*(uint*)sp);

  asm("movl %1, %%esp;" :"=r"(sp) :"r"(sp) :);
  asm("movl %%esp, %0;" :"=r"(temp) :"r"(temp) :);

  printf(1, "in uthread_exit starting temp123: %x temp: %x  *temp: %x\n",temp123, temp, *(uint*)temp);

  alarm(UTHREAD_QUANTA);
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
