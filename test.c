#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int psz = 4096;
  int* pages[30];

  for(int n = 1; n < 30; n++){
    printf(1,"allocating %d pages\n", n);
    for(int i = 0; i < n; i++){
      if((pages[i] = malloc(psz)) == 0)
        printf(1,"  FAILED to allocate %d pages\n", n);
      printf(1,"  writing %d to %d:%d\n",i,i,pages[i]);
      *(pages[i]) = i;
      //printf(1,"  now there is %d in %d:%d\n",*(pages[i]),i,pages[i]);
    }
    printf(1,"freeing %d pages\n", n);
    for(int i = 0; i < n; i++){
      printf(1,"  freeing %d:%d\n",i,pages[i]);
      free(pages[i]);
    }
  }

  exit();
}
