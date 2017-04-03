#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int i = 0;
  if((i += 10) == 10){
    printf(1,"GOOD %d\n",i);
  }
  else {
    printf(1,"BAD %d\n",i);
  }
  printf(1,"Hello World!\n");
  return (0);
}
