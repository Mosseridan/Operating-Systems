#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  int status;
  argint(0,&status);
  exit(status);
  return 0;  // not reached
}

int
sys_wait(void)
{
  int *pstatus;
  argptr(0, (void*)&pstatus, sizeof(*pstatus));
  return wait(pstatus);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sets the calling process's ntickets to to the given priority and updates the total ticket count in total_ntickets
int
sys_priority(void)
{
  int new_priority;
  argint(0,&new_priority);
  priority(new_priority);
  return 0;
}

// change scheduling policy and update ticket distribution acordingly
int
sys_policy(void)
{
  int new_policy;
  argint(0,&new_policy);
  if (new_policy > 2 || new_policy < 0) return -1;
  policy(new_policy);
  return 0;
}
