#include "types.h"
#include "x86.h"
#include "param.h"


enum tstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// struct uthread_linked_list{
//   struct uthread_linked_list_link* first;//pointer to the first link of the list
//   struct uthread_linked_list_link* last;//pointer to the last link in the list - used for adding
//   int length;// length of the list
// }
//
// struct uthread_linked_list_link{
//   struct uthread* data;//a pointer to the uthread that is represented by this link
//   struct uthread_linked_list_link* next;//a pointer to the next link in the list
// }

struct uthread {
  uint tid;   //thread id
  uint tstack; //pointer to the thread's stack
  uint uttable_index;
  enum tstate state; //thread state (UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE)
  struct trapframe tf; //the trapframe backed up on the user stack
  uint pid; //the pid of the process running this thread
  struct uthread* joined_on_me[MAX_UTHREADS-1]; //an array of pointers to uthreads that are joined on uthread
  // int joined_on; //a counter of uthreads that uthread is joined on
  // struct uthread_linked_list joined_on_me; //a linked list of pointers to uthreads that are joined on uthread
  // struct uthread*[MAX_UTHREADS-1] joined_on; //an array of pointers to uthreads that uthread is joined on
};

//uthread.cei
int uthread_init(void);
int uthread_create(void (*) (void*), void*);
void uthread_schedule(struct trapframe*);
void uthread_exit(void);
int uthread_self(void);
int uthread_join(int);
int uthread_sleep(int);
