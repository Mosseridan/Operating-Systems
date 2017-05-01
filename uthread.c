#include "types.h"
#include "user.h"
#include "x86.h"
#include "uthread.h"
#include "param.h"

uint nexttid = 1;
struct uthread* current;
struct uthread uttable[MAX_UTHREADS];

void
uthread_schedule_wrapper(int signum){
  uthread_schedule();//TODO: WTF?
}
//
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

int uthread_create(void (*start_func)(void*), void* arg)
{
  // struct uthread* nt = allocuthread();
  // if(!nt)
  //   return -1;
  //
  // uint sp = nt->tf->esp;
  // // push arg
  // sp -= 4;
  // *(void**)sp = arg;
  // // push argc
  // sp -= 4;
  // *(int*)sp = 1;
  // // push return address to thread_exit
  // sp -= 4;
  // *(void**)sp = uthread_exit;
  //
  // // reset threads esp in tf
  // nt->tf->esp = sp;
  // // set threads eip to start_func
  // nt->tf->eip = (uint)start_func;
  // //nt->eip = start_func; TODO: remove?
  //
  // return nt->tid;
return 0;
}

void uthread_schedule()
{
  struct uthread *ut = current;

  uint temp;
  asm("movl %%esp, %0;" :"=r"(temp) : :);
  ut->tf = (struct trapframe*)(temp + 12);
  ut->esp = temp;
  asm("movl %%ebp, %0;" :"=r"(temp) : :);
  ut->ebp = temp;


  ut->state = RUNNABLE;
  //  printf(1,"1in uthread_schedule tf: %x tf->eip: %x\n",ut->tf, ut->tf->eip);

  ut++;
  while(ut->state != RUNNABLE){
    ut++;
    if(ut >= &uttable[MAX_UTHREADS])
      ut = uttable;
  }

  // temp = ut->tf->ebp;
  // asm("movl %1, %%ebp;" :"=r"(temp) :"r"(temp) :);
  temp = ut->ebp;
  asm("movl %1, %%ebp;" :"=r"(temp) :"r"(temp) :);
  temp = ut->esp;
  asm("movl %1, %%esp;" :"=r"(temp) :"r"(temp) :);
  //printf(1,"2in uthread_schedule temp: %x\n",temp);

  //
  // asm("movl %%esp, %0;" :"=r"(temp) : :);
  // printf(1,"3in uthread_schedule temp: %x\n",temp);

  alarm(UTHREAD_QUANTA);
  return;
}

void uthread_exit()
{

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
