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

#define CURRENTDIR    0
#define PARENTDIR     1
#define BLOCKSTAT     2
#define INODESTAT     3
#define FIRSTPID      4

#define CURRENTDIRSTR "."
#define PARENTDIRSTR ".."
#define BLOCKSTATSTR "blockstat"
#define INODESTATSTR "inodestat"

#define T_PROC            1
#define T_PROC_BLOCKSTAT  2
#define T_PROC_INODESTAT  3
#define T_PROC_PID        4
#define T_PROC_PID_CWD    5
#define T_PROC_PID_FDINFO 6
#define T_PROC_PID_STATUS 7


// #define ISDIR 0x80000000


int procfsreadProc(struct inode *ip, char *dst, int off, int n);
int procfsreadPid(struct inode *ip, char *dst, int off, int n);

int pidToString(int pid, char *str);
void intToString(int n, int len, char *str);
int strcmp(const char *p, const char *q);

struct ptable{
  struct spinlock lock;
  struct proc proc[NPROC];
};

char *procfs_proc_pid_names[7] = { ".", "..", "cwd", "fdinfo", "status"}; // Used to create the PID directories // TODO: use this or remove

int
procfsisdir(struct inode *ip)
{
  if(ip->type == T_DEV && ip->major == PROCFS && (ip->minor == T_PROC || ip->minor == T_PROC_PID || ip->minor == T_PROC_PID_FDINFO)) // All of our direcetories are devices with minor=T_DIR (because we need them to use procnfsiread before ilock)
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
  // if(ip->inum & ISDIR) // We manually set BLOCKSTAT to have inum BLOCKSTAT(=2)+MAX_DINODES(=200) and inodestat to have inum INODESTAT(=3)+MAX_DINODES(=200)//TODO: IS THIS CORRECT?
  //   ip->minor = T_DIR;
  // else
  //   ip->minor = T_FILE;
  //cprintf("procfsiread: ip->minor: %d\n", ip->minor);
  ip->flags |= I_VALID; // To prevent ilock from trying to read data from the disk (because there is no data on the disk!)
}

int
procfsread(struct inode *ip, char *dst, int off, int n)
{
	if (ip->minor == T_PROC){
    // 1. ip is the inode describing "/proc"
      return procfsreadProc(ip, dst, off, n);
  } else if(ip->minor == T_PROC_PID){
    // 2. ip is the inode describing "/proc/PID"
      return procfsreadPid(ip, dst, off, n);
  // } else if(){
  //   // 3. ip is the inode describing "/proc/PID/cwd"
  // } else if(){
  //   // 4. ip is the inode describing "/proc/PID/status"
  // } else if(){
  //   // 5. ip is the inode describing "/proc/PID/fdinfo"
  // } else if(){
  //   // 6. ip is the inode describing "/proc/PID/fdinfo/FD"
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
  struct proc* p;
  struct ptable pt;
  struct dirent dirent;
  uint index;
  uint countProcs;

  if(off == 0){
      index = 0;
  } else
      index = off/sizeof(dirent);

  //cprintf("procfsreadProc: ip->inum: %d , off: %d, n: %d\n", ip->inum, off, n);

  switch(index){
    case CURRENTDIR:
      dirent.inum = ip->inum;
      dirent.type = ip->minor;
      strncpy(dirent.name, CURRENTDIRSTR, sizeof(CURRENTDIRSTR));
      break;
    case PARENTDIR:
      dirent.inum = PARENTDIR+MAX_DINODES;
      dirent.type = 0;
      // dirent.inum |= ISDIR;
      strncpy(dirent.name, PARENTDIRSTR, sizeof(PARENTDIRSTR));
      break;
    case BLOCKSTAT:
      dirent.inum = BLOCKSTAT+MAX_DINODES;
      dirent.type = T_PROC_BLOCKSTAT;
      strncpy(dirent.name, BLOCKSTATSTR, sizeof(BLOCKSTATSTR));
      break;
    case INODESTAT:
      dirent.inum = INODESTAT+MAX_DINODES;
      dirent.type = T_PROC_INODESTAT;
      strncpy(dirent.name, INODESTATSTR, sizeof(INODESTATSTR));
      break;
    default:
      countProcs = FIRSTPID;
      pt = getPtable();
      acquire(&pt.lock);
      for(p = pt.proc; p < &pt.proc[NPROC] && countProcs <= index; p++){
        // cprintf("procfsreadProc: p->pid: %d , index: %d, countProcs: %d\n", p->pid, index, countProcs);

        if(p->state != UNUSED && p->state != ZOMBIE && index == countProcs++){
          //cprintf("procfsreadProcIF: p->pid: %d , index: %d, countProcs: %d\n", p->pid, index, countProcs);
          dirent.inum = (p->pid + MAX_DINODES);
          dirent.type = T_PROC_PID;
          // dirent.inum |= ISDIR;
          if(pidToString(p->pid, dirent.name) == -1)
            panic("procfsread: pid exceeds 12 digits");
          break;
        }
      }
      release(&pt.lock);
      return 0;
  }
  memmove(dst, &dirent , n);
  return n;
}

int
procfsreadPid(struct inode *ip, char *dst, int off, int n)
{
  struct proc* p;
  struct ptable pt;
  struct dirent dirent;
  uint index;
  uint countProcs;

  if(off == 0){
      index = 0;
  } else
      index = off/sizeof(dirent);

  switch(index){
    case CURRENTDIR:
      dirent.inum = ip->inum;
      strncpy(dirent.name, CURRENTDIRSTR, sizeof(CURRENTDIRSTR));
      break;
    case PARENTDIR:
      dirent.inum = PARENTDIR+MAX_DINODES;
      strncpy(dirent.name, PARENTDIRSTR, sizeof(PARENTDIRSTR));
      break;
    case BLOCKSTAT:
      dirent.inum = BLOCKSTAT+MAX_DINODES;
      dirent.type = T_PROC_BLOCKSTAT;
      strncpy(dirent.name, BLOCKSTATSTR, sizeof(BLOCKSTATSTR));
      break;
    case INODESTAT:
      dirent.inum = INODESTAT+MAX_DINODES;
      dirent.type = T_PROC_INODESTAT;
      strncpy(dirent.name, INODESTATSTR, sizeof(INODESTATSTR));
      break;
  //   default:
  //     countProcs = FIRSTPID;
  //     pt = getPtable();
  //     acquire(&pt.lock);
  //     for(p = pt.proc; p < &pt.proc[NPROC] && countProcs <= index; p++){
  //       // cprintf("procfsreadProc: p->pid: %d , index: %d, countProcs: %d\n", p->pid, index, countProcs);
  //
  //       if(p->state != UNUSED && p->state != ZOMBIE && index == countProcs++){
  //         //cprintf("procfsreadProcIF: p->pid: %d , index: %d, countProcs: %d\n", p->pid, index, countProcs);
  //         dirent.inum = (p->pid + MAX_DINODES);
  //         dirent.type = T_PROC_PID;
  //         // dirent.inum |= ISDIR;
  //         if(pidToString(p->pid, dirent.name) == -1)
  //           panic("procfsread: pid exceeds 12 digits");
  //         break;
  //       }
  //     }
  //     release(&pt.lock);
  //     return 0;
  // }
  memmove(dst, &dirent , n);
  return n;
  return 0;
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
