#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

int
procfsisdir(struct inode *ip) {
  if(ip->type == T_DIR)
    return T_DIR; //This returns 1 (T_DIR = 1)
  return 0;
}

void
procfsiread(struct inode* dp, struct inode *ip) {
  // ip->type = //WHERE DO I FIND THIS?
  if(ip->type == T_DEV){
    //major and minor are only relevant if this inode represents a device
    // ip->major = //WHERE DO I FIND THIS?
    // ip->minor = //WHERE DO I FIND THIS?
  } else if(ip->type == 0)
    panic("procfsiread: no type");
  ip->flags |= I_VALID;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
  // struct ptable* pt = getPtable();
  cprintf("TESTING");
  return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  return 0;
}

void
procfsinit(void)
{
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}

void
addToFileSystem(int pid)
{
  //TODO: DO WE NEED THIS???
  //TODO: IMPLEMENT THIS
}

void
removeFromFileSystem(int pid)
{
  //TODO: DO WE NEED THIS???
  //TODO: IMPLEMENT THIS
}
