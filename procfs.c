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
}


// void
// ilock(struct inode *ip)
// {
//   struct buf *bp;
//   struct dinode *dip;
//
//   if(ip == 0 || ip->ref < 1)
//     panic("ilock");
//
//   acquire(&icache.lock);
//   while(ip->flags & I_BUSY)
//     sleep(ip, &icache.lock);
//   ip->flags |= I_BUSY;
//   release(&icache.lock);
//
//   if(!(ip->flags & I_VALID)){
//     bp = bread(ip->dev, IBLOCK(ip->inum));
//     dip = (struct dinode*)bp->data + ip->inum%IPB;
//     ip->type = dip->type;
//     ip->major = dip->major;
//     ip->minor = dip->minor;
//     ip->nlink = dip->nlink;
//     ip->size = dip->size;
//     memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
//     brelse(bp);
//     ip->flags |= I_VALID;
//     if(ip->type == 0)
//       panic("ilock: no type");
//   }
// }



int
procfsread(struct inode *ip, char *dst, int off, int n) {
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
