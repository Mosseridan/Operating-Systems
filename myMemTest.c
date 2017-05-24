#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int n = 26;
  printf(1,"\n\n\n!!! allocating a = malloc(%d)\n\n\n", 4096*n);
  int* a = malloc(4096*n);
  printf(1,"\n\n\n!!! a is alocated to: %d\n\n\n", a);

  for(int i = 0; i<1024; i++){
    for(int j = 0; j<n; j++){
      printf(1,"\n\n!!! writing to byte %d:%d\n\n",j,&(a[1024*j+i]));
      a[1024*j+i]=j;
    }
  }
  exit();
}
