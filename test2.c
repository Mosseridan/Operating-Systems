#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int n = 26;
  int* a[n];
  for(int i = 0; i<n; i++){
    a[i]= malloc(4096);
    printf(1,"!!! a[%d] is alocated to: %d\n", a);
    printf(1,"!!! writing to page %d:%d\n",i,&(a[i][j]));
    for(int j = 0; j<1024; j++)
      a[i][j] = j;
  }
  exit();
}
