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

#define PROC_FIRSTPID          4
#define PROC_PID_FIRSTFD       2

#define CURRENTDIR_STR       "."
#define PARENTDIR_STR        ".."
#define PROC_BLOCKSTAT_STR   "blockstat"
#define PROC_INODESTAT_STR   "inodestat"
#define PROC_PID_CWD_STR     "cwd"
#define PROC_PID_FDINFO_STR  "fdinfo"
#define PROC_PID_STATUS_STR  "status"

#define PROC_NUM_SHIFT    0x4

// #define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)

struct ptable{
  struct spinlock lock;
  struct proc proc[NPROC];
};

struct status{
  enum procstate state;
  uint size;
};

struct fd {
  uint type;
  int ref; // reference count
  char readable;
  char writable;
  uint inum;
  uint off;
};

struct blockstat {
  uint total_blocks;
  uint free_blocks;
  uint num_of_access;
  uint num_of_hits;
};

struct inodestat {
  ushort total;
  ushort free;
  ushort valid;
  uint refs;
  ushort used;
};


int procfsreadProc(struct inode *ip, char *dst, int off, int n);
int procfsreadPid(struct inode *ip, char *dst, int off, int n);
int procfsreadFdinfo(struct inode *ip, char *dst, int off, int n);
int procfsreadBlockstat(struct inode *ip, char *dst, int off, int n);
int procfsreadFD(struct inode *ip, char *dst, int off, int n);
int procfsreadInodestat(struct inode *ip, char *dst, int off, int n);
int procfsreadStatus(struct inode *ip, char *dst, int off, int n);
int trimBufferAndSend(void* buf, uint bytes_read, char *dst, int off, int n);
int intToString(int num, char *str);
void buildIntString(int n, int len, char *str);
int strcmp(const char *p, const char *q);


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
  } else if((ip->inum & 0xF000) == T_PROC_BLOCKSTAT){
    // 4. ip is the inode describing "/proc/PID/blockstat"
    return procfsreadBlockstat(ip, dst, off, n);
  } else if((ip->inum & 0xF000) == T_PROC_INODESTAT){
    // 5. ip is the inode describing "/proc/PID/inodestat"
    return procfsreadInodestat(ip, dst, off, n);
  } else if((ip->inum & 0xF000) == T_PROC_PID_FDINFO_FD){
    // 6. ip is the inode describing "/proc/PID/fdinfo/FD"
    return procfsreadFD(ip, dst, off, n);
  } else if((ip->inum & 0xF000) == T_PROC_PID_STATUS){
    // 7. ip is the inode describing "/proc/PID/status"
    return procfsreadStatus(ip, dst, off, n);
  }

  panic("procfsread: NO CASE WAS FOUND!\n");
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
  struct ptable* pt;
  struct dirent dirent;
  uint index, i, count_procs;

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
      count_procs = PROC_FIRSTPID;
      pt = getPtable();
      acquire(&pt->lock);
      for(i = 0 ; i<NPROC; i++){
        p = pt->proc + i;
        if(p->state != UNUSED && p->state != ZOMBIE && index == count_procs++){
          if(intToString(p->pid, dirent.name) == -1)
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
  uint index, pindex;
  struct proc* p;
  struct ptable* pt;

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
      pindex = ip->inum & 0xFF;
      pt = getPtable();
      acquire(&pt->lock);
      p = pt->proc + pindex;
      if(p->state == UNUSED || p->state == ZOMBIE){
        release(&pt->lock);
        return 0;
      }
      dirent.inum = p->cwd->inum;
      release(&pt->lock);
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
      count_fds = PROC_PID_FIRSTFD;
      acquire(&pt->lock);
      p = pt->proc + proc_num;
      if(p->state == UNUSED || p->state == ZOMBIE){
        release(&pt->lock);
        return 0;
      }
      for(fd = 0; fd<NOFILE; fd++){
        if(p->ofile[fd] && p->ofile[fd]->type==FD_INODE && index == count_fds++){
          if(intToString(fd, dirent.name) == -1)
            panic("procfsread: pid exceeds the max number of digits");
          dirent.inum = ((ip->inum & 0xFF) << PROC_NUM_SHIFT) | fd | T_PROC_PID_FDINFO_FD;
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

int
procfsreadBlockstat(struct inode *ip, char *dst, int off, int n)
{
  struct blockstat blockstat;
  getBlockstat(&blockstat);
  return trimBufferAndSend(&blockstat, sizeof(blockstat), dst, off, n);
}

int
procfsreadInodestat(struct inode *ip, char *dst, int off, int n)
{
  struct inodestat inodestat;
  getInodestat(&inodestat);
  return trimBufferAndSend(&inodestat, sizeof(inodestat), dst, off, n);
}

int
procfsreadFD(struct inode *ip, char *dst, int off, int n)
{
  uint pindex, pfdindex;
  struct proc* p;
  struct ptable* pt;
  struct file* fd;
  struct fd pfd;

  pindex = (ip->inum&0x0FF0) >> PROC_NUM_SHIFT;
  pfdindex = ip->inum & 0xF;

  pt = getPtable();
  acquire(&pt->lock);
    p = pt->proc + pindex;
    if(p->state == UNUSED || p->state == ZOMBIE){
      release(&pt->lock);
      return 0;
    }

    fd = p->ofile[pfdindex];
    pfd.type = fd->type;
    pfd.ref = fd->ref;
    pfd.readable = fd->readable;
    pfd.writable = fd->writable;
    if(fd->type == FD_INODE)
      pfd.inum = fd->ip->inum;
    pfd.off = fd->off;
  release(&pt->lock);
  return trimBufferAndSend(&pfd, sizeof(pfd), dst, off, n);
}

int
procfsreadStatus(struct inode *ip, char *dst, int off, int n)
{
  uint pindex;
  struct proc* p;
  struct ptable* pt;
  struct status pstatus;

  pindex = ip->inum & 0xFF;

  pt = getPtable();
  acquire(&pt->lock);
    p = pt->proc + pindex;
    pstatus.state = p->state;
    pstatus.size = p->sz;
  release(&pt->lock);
  return trimBufferAndSend(&pstatus, sizeof(pstatus), dst, off, n);
}

int
trimBufferAndSend(void* buf, uint bytes_read, char *dst, int off, int n)
{
  int bytes_to_send = 0;
  if (off < bytes_read) {
    bytes_to_send = bytes_read - off;
    if (bytes_to_send < n) {
      memmove(dst,(char*)buf+off,bytes_to_send);
      return bytes_read;
    }
    memmove(dst,(char*)buf+off,n);
    return n;
  }
  return 0;
}

int
intToString(int num, char *str)
{
  int temp, len, i;
	temp = num;
	len = 1;
	while (temp/10!=0){
		len++;
		temp /= 10;
	}
  if(len > DIRSIZ){
    cprintf("pidToString: Directory name should not exceed %d characters but this PID exceeds %d digits", DIRSIZ, DIRSIZ);
    return -1;
  }
	for (i = len; i > 0; i--){
		str[i-1] = (num%10)+48;
		num/=10;
	}
	str[len]='\0';

  return len;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}
