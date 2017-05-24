#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int n = 26;
  int* a = malloc(4096*n);
  printf(1,"test1 !!! a is alocated to: %d\n", a);
  for(int i = 0; i<n; i++){
    printf(1,"test1 !!! writing to page %d:%d\n",i,&(a[1024*i]));
    for(int j = 0; j<1024; j++)
      a[1024*i+j]=j;
  }
  exit();
}
