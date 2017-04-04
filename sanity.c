#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int ntypes = 3;
  int nprocesses = 4;
  int npolicies = 3;
  int total_processes = ntypes * nprocesses;
  int cpids[total_processes];
  int status[total_processes];
  struct perf perfs[total_processes];
  int cpid;

  for(int j = 0; j < npolicies ; j++){

    policy(j);

    // run CPU intensive consecutively
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/fib", argv);
          exit(0);
      }
      else{
        printf(1,"consecutive policy %d CPU intensive %d: pid = %d\n",j,i,cpid);
        cpids[i] =  wait_stat(&(status[i]),&(perfs[i]));
      }
    }

    // run I/O intensive consecutively
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/sleepy", argv);
          exit(0);
      }
      else{
        printf(1,"consecutive policy %d I0 intensive %d: pid = %d\n",j,i,cpid);
        cpids[i+nprocesses] =  wait_stat(&(status[i+nprocesses]),&(perfs[i+nprocesses]));
      }
    }

    // run mixed consecutively
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/halflife", argv);
          exit(0);
      }
      else{
        printf(1,"consecutive policy %d MIXED %d: pid = %d\n",j,i,cpid);
        cpids[i+2*nprocesses] =  wait_stat(&(status[i+2*nprocesses]),&(perfs[i+2*nprocesses]));
      }
    }
    printf(1,"\n");

    for(int i = 0; i < total_processes; i++){
      printf(1,"@ pid = %d ,status = %d, ctime = %d, ttime = %d, stime = %d, retime = %d, rutime = %d, tatime = %d\n"
              ,cpids[i] , status[i], perfs[i].ctime, perfs[i].ttime, perfs[i].stime, perfs[i].retime, perfs[i].rutime, perfs[i].ttime - perfs[i].ctime);
    }
    printf(1,"\n");

    // run CPU intensive paralel
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/fib", argv);
          exit(0);
      }
      else printf(1,"paralel policy %d CPU intensive %d: pid = %d\n",j,i,cpid);
    }

    // run I/O intensive paralel
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/sleepy", argv);
          exit(0);
      }
      else printf(1,"paralel policy %d I0 intensive %d: pid = %d\n",j,i,cpid);
    }

    // run mixed paralel
    for(int i = 0; i < nprocesses; i++){
      if((cpid = fork()) == 0){
          exec("bin/halflife", argv);
          exit(0);
      }
      else printf(1,"paralel policy %d MIXED %d: pid = %d\n",j,i,cpid);
    }
    printf(1,"\n");

    for(int i = 0; i < total_processes; i++){
      cpids[i] =  wait_stat(&(status[i]),&(perfs[i]));
      printf(1,"@ pid = %d ,status = %d, ctime = %d, ttime = %d, stime = %d, retime = %d, rutime = %d, tatime = %d\n"
              ,cpids[i] , status[i], perfs[i].ctime, perfs[i].ttime, perfs[i].stime, perfs[i].retime, perfs[i].rutime, perfs[i].ttime - perfs[i].ctime);
    }
    printf(1,"\n");
  }
  return (0);
}
