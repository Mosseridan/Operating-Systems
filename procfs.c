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

#define CURRENTDIR         0
#define PARENTDIR          1
#define PROC_BLOCKSTAT     2
#define PROC_INODESTAT     3
#define PROC_PID_CWD       2
#define PROC_PID_FDINFO    3
#define PROC_PID_STATUS    4

#define PROC_FIRSTPID      4

#define CURRENTDIR_STR       "."
#define PARENTDIR_STR        ".."
#define PROC_BLOCKSTAT_STR   "blockstat"
#define PROC_INODESTAT_STR   "inodestat"
#define PROC_PID_CWD_STR     "cwd"
#define PROC_PID_FDINFO_STR  "fdinfo"
#define PROC_PID_STATUS_STR  "status"

#define T_PROC               0x1000
#define T_PROC_BLOCKSTAT     0x2000
#define T_PROC_INODESTAT     0x3000
#define T_PROC_PID           0x4000
#define T_PROC_PID_CWD       0x5000
#define T_PROC_PID_FDINFO    0x6000
#define T_PROC_PID_STATUS    0x7000
#define T_PROC_PID_FDINFO_FD 0x8000

#define PROC_NUM_SHIFT    0x4

// #define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)

int procfsreadProc(struct inode *ip, char *dst, int off, int n);
int procfsreadPid(struct inode *ip, char *dst, int off, int n);
int procfsreadFdinfo(struct inode *ip, char *dst, int off, int n);
int pidToString(int pid, char *str);
void intToString(int n, int len, char *str);
int strcmp(const char *p, const char *q);

struct ptable{
  struct spinlock lock;
  struct proc proc[NPROC];
};


int
procfsisdir(struct inode *ip)
{
  if(ip->type == T_DEV &&
    ip->major == PROCFS &&
    ((ip->inum & 0xF000) == T_PROC_PID ||
      (ip->inum & 0xF000) == T_PROC_PID_FDINFO ||
      namei("proc")->inum == ip->inum));
    return 1;
  return 0;
}

void
procfsiread(struct inode* dp, struct inode *ip)
{
  if(ip->inum < MAX_DINODES) // If inum < MAX_DINODES it means that this is a regular system inode (the data is on the physical disk)
    return; // Return and call ilock to get the data from the disk
  ip->type = T_DEV; // All of our direcetories are devices with minor=T_DIR (because we need them to use procnfsiread before ilock)
  ip->ref = 1;
  ip->major = PROCFS;
  ip->flags |= I_VALID; // To prevent ilock from trying to read data from the disk (because there is no data on the disk!)
}

int
procfsread(struct inode *ip, char *dst, int off, int n)
{
	if (namei("proc")->inum ==  ip->inum){
    // 1. ip is the inode describing "/proc"
      return procfsreadProc(ip, dst, off, n);
  } else if((ip->inum & 0xF000) == T_PROC_PID){
    // 2. ip is the inode describing "/proc/PID"
      return procfsreadPid(ip, dst, off, n);
  } else if((ip->inum & 0xF000) == T_PROC_PID_FDINFO){
    // 3. ip is the inode describing "/proc/PID/fdinfo"
    return procfsreadFdinfo(ip, dst, off, n);
  }

  cprintf("procfsread: DOING NOTHING\n"); //TODO: remove this!!!
  return 0;
}


int
procfswrite(struct inode *ip, char *buf, int n)
{
  panic ("procfswrite: Tried to write in a read only system!\n");
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

/**********************************************/
/**************Auxilary Functions**************/
/**********************************************/

//procfsread cases:
int
procfsreadProc(struct inode *ip, char *dst, int off, int n)
{
  // cprintf("!!!!! offset: %d\n",off);

  struct proc* p;
  struct ptable* pt;
  struct dirent dirent;
  uint index, i, countProcs;

  if(off == 0){
      index = 0;
  } else
      index = off/sizeof(dirent);

  switch(index){
    case CURRENTDIR:
      dirent.inum = ip->inum;
      strncpy(dirent.name, CURRENTDIR_STR, sizeof(CURRENTDIR_STR));
      break;
    case PARENTDIR:
      dirent.inum = namei("/")->inum;
      strncpy(dirent.name, PARENTDIR_STR, sizeof(PARENTDIR_STR));
      break;
    case PROC_BLOCKSTAT:
      dirent.inum = T_PROC_BLOCKSTAT;
      strncpy(dirent.name, PROC_BLOCKSTAT_STR, sizeof(PROC_BLOCKSTAT_STR));
      break;
    case PROC_INODESTAT:
      dirent.inum = T_PROC_INODESTAT;
      strncpy(dirent.name, PROC_INODESTAT_STR, sizeof(PROC_INODESTAT_STR));
      break;
    default:
      countProcs = PROC_FIRSTPID;
      pt = getPtable();
      acquire(&pt->lock);
      for(i = 0 ; i<NPROC; i++){
        p = pt->proc + i;
        // cprintf("@@@@@@@@@ i: %d index: %d countProcs: %d pid: %d name: %s\n", i, index, countProcs, p->pid, p->name);
        if(p->state != UNUSED && p->state != ZOMBIE && index == countProcs++){
          if(pidToString(p->pid, dirent.name) == -1)
            panic("procfsread: pid exceeds the max number of digits");
          dirent.inum = i | T_PROC_PID;
          break;
        }
      }
      release(&pt->lock);
      if(i >= NPROC)
        return 0;
  }
  memmove(dst, &dirent , n);
  return n;
}


int
procfsreadPid(struct inode *ip, char *dst, int off, int n)
{
  struct dirent dirent;
  uint index;

  if(off == 0){
      index = 0;
  } else
      index = off/sizeof(dirent);

  switch(index){
    case CURRENTDIR:
      dirent.inum = ip->inum;
      strncpy(dirent.name, CURRENTDIR_STR, sizeof(CURRENTDIR_STR));
      break;
    case PARENTDIR:
      dirent.inum = namei("proc")->inum;
      strncpy(dirent.name, PARENTDIR_STR, sizeof(PARENTDIR_STR));
      break;
    case PROC_PID_CWD:
      dirent.inum =  (ip->inum & 0xFF) | T_PROC_PID_CWD;
      strncpy(dirent.name, PROC_PID_CWD_STR , sizeof(PROC_PID_CWD_STR));
      break;
    case PROC_PID_FDINFO:
      dirent.inum =  (ip->inum & 0xFF) | T_PROC_PID_FDINFO;
      strncpy(dirent.name, PROC_PID_FDINFO_STR , sizeof(PROC_PID_FDINFO_STR));
      break;
    case PROC_PID_STATUS:
      dirent.inum =  (ip->inum & 0xFF) | T_PROC_PID_STATUS;
      strncpy(dirent.name, PROC_PID_STATUS_STR , sizeof(PROC_PID_STATUS_STR));
      break;
    default:
    // panic("procfsreadPid: no dirent matching the requested offset!");
      return 0;
  }
  memmove(dst, &dirent , n);
  return n;
}


int
procfsreadFdinfo(struct inode *ip, char *dst, int off, int n)
{
  struct dirent dirent;
  uint index, fd, proc_num, count_fds;
  struct ptable* pt;
  struct proc* p;

  if(off == 0){
      index = 0;
  } else
      index = off/sizeof(dirent);

  switch(index){
    case CURRENTDIR:
      dirent.inum = ip->inum;
      strncpy(dirent.name, CURRENTDIR_STR, sizeof(CURRENTDIR_STR));
      break;
    case PARENTDIR:
      dirent.inum = (ip->inum & 0xFF) | T_PROC_PID;
      strncpy(dirent.name, PARENTDIR_STR, sizeof(PARENTDIR_STR));
      break;
    default:
      proc_num = ip->inum & 0xFF;
      pt = getPtable();
      acquire(&pt->lock);
      p = pt->proc + proc_num;
      if(p->state == ZOMBIE || p->state == UNUSED)
        panic("procfsreadFdinfo: trying to fread fdinfo from an unused process");
      for(fd = 0; fd<NOFILE; fd++){
        // cprintf("@@@@@@@@@ i: %d index: %d countProcs: %d pid: %d name: %s\n", i, index, countProcs, p->pid, p->name);
        if(p->ofile[fd] && index == count_fds++){
          if(pidToString(fd, dirent.name) == -1)
            panic("procfsread: pid exceeds the max number of digits");
          dirent.inum = ((ip->inum & 0xFF) << PROC_NUM_SHIFT) | fd | T_PROC_PID_FDINFO_FD;
          //dirent.inum |= (fd | T_PROC_PID_FDINFO_FD);
          break;
        }
      }
      release(&pt->lock);
      if(fd >= NOFILE)
        return 0;
  }
  memmove(dst, &dirent , n);
return n;
}


//end of procfsread cases:

int
pidToString(int pid, char *str)
{
  int temp, len;
	temp = pid;
	len = 1;
	while (temp/10!=0){
		len++;
		temp /= 10;
	}
  if(len > DIRSIZ){
    cprintf("pidToString: Directory name should not exceed %d characters but this PID exceeds %d digits", DIRSIZ, DIRSIZ);
    return -1;
  }
  intToString(pid, len, str);
  return 0;
}

void
intToString(int n, int len, char *str)
{
  int i;
	for (i = len; i > 0; i--){
		str[i-1] = (n%10)+48;
		n/=10;
	}
	str[len]='\0';
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}
