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


#define PARENTDIR 1
#define BLOCKSTAT 2
#define INODESTAT 3

// int procfsreadProc(struct inode *_ip, char *_dst, int _off, int _n, struct proc* _p, struct ptable _pt, struct dirent* _proc_dirents); //procfsreadProc(struct inode *_ip, char *_dst, int _off, int _n);
int procfsreadProc(struct inode *ip, char *dst, int off, int n);
int procfsreadBlockstat(struct inode *ip, char *dst, int off, int n);
int procfsreadInodestat(struct inode *ip, char *dst, int off, int n);
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
  if(namei("proc") == ip || (ip->type == T_DEV && ip->major == PROCFS && ip->minor == T_DIR)) // All of our direcetories are devices with minor=T_DIR (because we need them to use procnfsiread before ilock)
    return T_DIR; //This returns 1 (T_DIR = 1)
  return 0;
}

void
procfsiread(struct inode* dp, struct inode *ip)
{
  if(ip->inum < INODE_FIRST_INDEX) // If inum < INODE_FIRST_INDEX it means that this is a regular system inode (the data is on the physical disk)
    return; // Return and call ilock to get the data from the disk
  ip->type = T_DEV; // All of our direcetories are devices with minor=T_DIR (because we need them to use procnfsiread before ilock)
  ip->ref = 1;
  ip->major = PROCFS;
  if(ip->inum == BLOCKSTAT || ip->inum == INODESTAT) // We manually set BLOCKSTAT to have inum BLOCKSTAT(=2)+INODE_FIRST_INDEX(=200) and inodestat to have inum INODESTAT(=3)+INODE_FIRST_INDEX(=200)//TODO: IS THIS CORRECT?
    ip->minor = T_FILE;
  else
    ip->minor = T_DIR;
  ip->flags |= I_VALID; // To prevent ilock from trying to read data from the disk (because there is no data on the disk!)
}

int
procfsread(struct inode *ip, char *dst, int off, int n)
{
  // struct proc* p;
  // struct ptable pt;
  // struct dirent proc_dirents[NPROC+4]; // Represents the direntries inside /proc, NPROC(=64) entries for the running processes, 2 for the . and .., and 2 for the blockstat and inodestat

	if (ip == namei("proc")){
    // 1. ip is the inode describing "/proc"
      return procfsreadProc(ip, dst, off, n);
  } else if(ip.inum == BLOCKSTAT){
    // 2. ip is the inode describing "/proc/blockstat"
      return procfsreadBlockstat(ip, dst, off, n);
  } else if(ip.inum == INODESTAT){
    // 3. ip is the inode describing "/proc/inodestat"
      return procfsreadInodestat(ip, dst, off, n);
  // } else if(){
  //   // 4. ip is the inode describing "/proc/PID"
  // } else if(){
  //   // 5. ip is the inode describing "/proc/PID/cwd"
  // } else if(){
  //   // 6. ip is the inode describing "/proc/PID/status"
  // } else if(){
  //   // 7. ip is the inode describing "/proc/PID/fdinfo"
  // } else if(){
  //   // 8. ip is the inode describing "/proc/PID/fdinfo/FD"
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
  struct dirent proc_dirents[NPROC+4]; // Represents the direntries inside /proc, NPROC(=64) entries for the running processes, 2 for the . and .., and 2 for the blockstat and inodestat
  int num_of_dirents = -1;

  // Treating the . , .. , blockstat and inodestat special cases seperately
  proc_dirents[++num_of_dirents].inum = ip->inum;
  strncpy(proc_dirents[num_of_dirents].name, ".", 2);

  proc_dirents[++num_of_dirents].inum = 1;
  strncpy(proc_dirents[num_of_dirents].name, "..", 3);

  // Treating the blockstat and inodestat special cases seperately
  proc_dirents[++num_of_dirents].inum = 2;
  strncpy(proc_dirents[num_of_dirents].name, "blockstat", 10);

  proc_dirents[++num_of_dirents].inum = 3;
  strncpy(proc_dirents[num_of_dirents].name, "inodestat", 10);

  // Treating the directories of all the running processes
  pt = getPtable();
  acquire(&pt.lock);
	for(p = pt.proc; p < &pt.proc[NPROC]; p++){
		if(p->state != UNUSED && p->state != ZOMBIE){
			if(pidToString(p->pid, proc_dirents[++num_of_dirents].name) == -1)
        panic("procfsread: pid exceeds 14 digits");
			proc_dirents[num_of_dirents].inum = p->pid + INODE_FIRST_INDEX;
		}
	}
	release(&pt.lock);

  if (off >= num_of_dirents*sizeof(struct dirent)) // The offset is out of bounds (no such dirent)
		return 0;

	memmove(dst, (char *)((uint)proc_dirents+(uint)off), n);
	return n;
}

int
procfsreadBlockstat(struct inode *ip, char *dst, int off, int n)
{
  return 0;
}

int
procfsreadInodestat(struct inode *ip, char *dst, int off, int n)
{
  return 0;
}

int
procfsreadPid(struct inode *ip, char *dst, int off, int n)
{
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
    cprintf("pidToString: Directory name should not exceed 14 characters but this PID exceeds 14 digits");
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
