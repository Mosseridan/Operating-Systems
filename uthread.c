#include "uthread.h"
#include "user.h"
#include "param.h"

uint nexttid = 1;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

void
uthread_schedule_wrapper(int signum){
  uthread_schedule();//TODO: WTF?
}

// static struct uthread*
// allocuthread()
// {
//   struct uthread *ut;
//   uint sp;
//
//   for(ut = uttable; ut < &uttable[MAX_UTHREADS]; ut++){
//     if(ut->state == UNUSED)
//       goto found;
//   }
//   return 0;
//
//   found:
//     ut->state = EMBRYO;
//     ut->tid = nexttid++;
//     ut->pid = getpid();
//
//     // Allocate thred stack.
//     if((ut->tf->ss = (uint)malloc(TSTACKSIZE)) == 0){
//       ut->state = UNUSED;
//       return 0;
//     }
//     sp = ut->tf->ss + TSTACKSIZE;
//
//
//     // Leave room for trap frame.
//     sp -= sizeof *ut->tf;
//     ut->tf = (struct trapframe*)sp;
//     memmove(ut->tf ,current->tf,sizeof(struct trapframe));
//     // initialize thread stack pointers
//     ut->ebp = sp;
//     ut->tf->ebp = sp;
//     ut->esp = sp;
//     ut->tf->esp = sp;
//     return ut;
// }

//TODO: Set EIP to be something we don't know just yet
int
uthread_init()
{
  struct uthread* ut = uttable;
  ut->tid = nexttid++;
  ut->pid = getpid();
  ut->state = RUNNING;
  current = ut;
  signal(SIGALRM, (sighandler_t) uthread_schedule_wrapper);
  alarm(UTHREAD_QUANTA);
  return ut->tid;
}

int
uthread_create(void (*start_func)(void*), void* arg)
{
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
    if((ut->tf.ss = (uint)malloc(TSTACKSIZE)) == 0){
      ut->state = UNUSED;
      return -1;
    }



    sp = ut->tf.ss + TSTACKSIZE;




    // // Allocate thread stack.
    // if((sp = (uint)malloc(TSTACKSIZE)) == 0){
    //   ut->state = UNUSED;
    //   return -1;
    // }
    //
    //
    //
    // sp += TSTACKSIZE;



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
    ut->tf.ebp = sp;
    //ut->esp = sp;
    ut->tf.esp = sp;

    // set threads eip to start_func
    ut->tf.eip = (uint)start_func;
    printf(1,"\nin uthread_create: current->tid: %d, ut->tid: %d, ut->tf.esp: %x\n"
    ,current->tid,ut->tid,ut->tf.esp);
    //printf(1,"!! current->tf.eip,"current->tf-<)
    ut->state = RUNNABLE;
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
  printf(1, "\nin uthread_schedule before current->tid: %d, ut->tid: %d, tf: %x\n",current->tid,ut->tid, tf);
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
   *((struct trapframe*)tf) = ut->tf;
   current = ut;
   ut->state = RUNNING;
   printf(1, "\nin uthread_schedule before current->tid: %d, ut->tid: %d tf: %x\n",current->tid, ut->tid, tf);
   alarm(UTHREAD_QUANTA);

   return;
}

void
uthread_exit()
{
  printf(1, "\nin uthread_exit\n");
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
