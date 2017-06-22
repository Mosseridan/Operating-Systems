#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

#define BSIZE      512
#define MAXPATHLEN 512
#define NOFILE     16


struct fdData {
  uint type;
  int ref; // reference count
  char readable;
  char writable;
  uint inum;
  uint off;
};

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
    return -1;
  }
	for (i = len; i > 0; i--){
		str[i-1] = (num%10)+48;
		num/=10;
	}
	str[len]='\0';

  return len;
}

void
printLsofPID(struct dirent* de)
{
  char path[MAXPATHLEN];
  char *p;
  int fd,i;
  struct fdData fdData;
  char* file_types[]= { "FD_NONE", "FD_PIPE", "FD_INODE" };

  strcpy(path, "/proc/");
  p = path + strlen(path);
  strcpy(p, de->name);
  p = path + strlen(path);
  strcpy(p, "/fdinfo/");
  p = path + strlen(path);

  for(i=0; i<NOFILE; i++){
    intToString(i, p);

    if((fd = open(path, 0)) < 0){
      continue;
    }

    read(fd, &fdData, sizeof(fdData));

    printf(1,"name: %s fd num: %d refs: %d inum: %d type: %s\n", de->name, i, fdData.ref, fdData.inum, file_types[fdData.type]);

    close(fd);
  }
}


int
main(int argc, char *argv[])
{
  struct dirent de;
  int fdproc, i;

  if((fdproc = open("proc", 0)) < 0){
    printf(1, "lsof: cannot open proc\n");
    return -1;
  }

  for(i=0; i<4; i++){ //Reading the first irelevent dirents
    read(fdproc, &de, sizeof(de));
  }

  while(read(fdproc, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    printLsofPID(&de);
  }

  close(fdproc);

  exit();
}
