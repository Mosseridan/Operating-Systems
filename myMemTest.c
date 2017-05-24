#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int n = 26;
  printf(1,"\n\nmyMemTest !!! allocating a = malloc(%d)\n\n", 4096*n);
  int* a = malloc(4096*n);
  printf(1,"\n\nmyMemTest!!! a is alocated to: %d\n\n", a);

  for(int i = 0; i<1024; i++){
    for(int j = 0; j<n; j++){
      if((i%100==0) || i==1023){
        printf(1,"myMemTest !!! writing to byte %d:%d\n",j,&(a[1024*j+i]));
        a[1024*j+i]=j;
      }
    }
  }
  exit();
}
