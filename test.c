#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

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

	int n = 5, k = 3;
	int ppid = getpid();

	if(fork()){
		for (int i = 0; i < n; i++){
			if(!fork()){
				// sleep(i*100);
				char path1[512];
				char path2[512];
				int pid = getpid();
				int fdinodestat = open("/proc/inodestat", 0);
				int fdblockstat = open("/proc/blockstat", 0);
				strcpy(path1, "/proc/");
				intToString(ppid, path1+strlen(path1));
				int fdpid = open(path1, 0);
				strcpy(path2, "/proc/");
				intToString(pid , path2+strlen(path2));
				fdpid = open(path2, 0);
				while(k){
					if(fork()){
						wait();
						k = 0;
					}
					else if(k){
						k--;
					}
				}
				sleep(i*100);
				close(fdinodestat);
				close(fdblockstat);
				close(fdpid);
				exit();
			}
		}
		for (int i = 0; i < n; i++){
			wait();
		}
	}
	else{
		for (int i = 0; i < n; i++){
			if(fork()){
				sleep(i*100);
				wait();
			}
			else{
				sleep(i*100);
				printf(1,"\n\n!! lsof %d:\n",i);
				exec("lsof",argv);
				exit();
			}
		}
		exit();
	}

	wait();
	exit();




	// int i, pid, ppid;
	// char path[30];
	// ppid = getpid();
	//
  // for(i=0; i<15; i++){
  //   pid=fork();
  //     if(i==10 || i==0 || i==14){
  //       if(pid==0){
	// 				printf(1,"\n\nstarting lsof:\n");
  //         exec("lsof", argv);
  //         exit();
  //       } else{
  //         wait();
  //       }
  //     }else if(pid==0){
	// 				int fdblockstat, fdinodestat, fdpid;
	// 				fdinodestat = open("/proc/inodestat", 0);
	// 			 	fdblockstat = open("/proc/blockstat", 0);
	// 				strcpy(path, "/proc/");
	// 				intToString(ppid, path+strlen(path));
	// 				fdpid = open(path, 0);
  //         fib(39);
	// 				close(fdinodestat);
	// 				close(fdblockstat);
	// 				close(fdpid);
  //         exit();
  //     }
  // }
	//
  // for(i=0; i<12; i++)
  //   wait();
	// exit();
}
