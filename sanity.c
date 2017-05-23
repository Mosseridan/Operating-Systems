#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{

  int cpid;
  if((cpid = fork()) == 0){
     exec("test1", argv);
  }
  wait();
  exit();
}
