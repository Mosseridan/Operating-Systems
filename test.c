#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

void fib(int n) {
	if (n <= 1)
		return;
	fib(n-1);
	fib(n-2);
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
main(int argc, char *argv[])
{
  int i, pid, ppid;
	char path[30];
	ppid = getpid();

  for(i=0; i<15; i++){
    pid=fork();
      if(i==10 || i==0 || i==14){
        if(pid==0){
					printf(1,"\n\nstarting lsof:\n");
          exec("lsof", argv);
          exit();
        } else{
          wait();
        }
      }else if(pid==0){
					int fdblockstat, fdinodestat, fdpid;
					fdinodestat = open("/proc/inodestat", 0);
				 	fdblockstat = open("/proc/blockstat", 0);
					strcpy(path, "/proc/");
					intToString(ppid, path+strlen(path));
					fdpid = open(path, 0);
          fib(39);
					close(fdinodestat);
					close(fdblockstat);
					close(fdpid);
          exit();
      }
  }

  for(i=0; i<12; i++)
    wait();
	exit();
}
