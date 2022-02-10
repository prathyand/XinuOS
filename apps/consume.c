#include <xinu.h>
#include <prodcons.h>
#include <prodcons_bb.h>
int n;
int32 arr_q[6];
int32 *tail;
void consumer(int count) {
  // TODO: implement the following:
  // - Iterates from 0 to count (count including)
  //   - reading the value of the global variable 'n' each time
  //   - print consumed value (the value of 'n'), e.g. "consumed : 8"
  int i;
  for(i=0;i<=count;i++){
    wait(prod);
    printf("consumed : %d\n",n);
    signal(cons);
    if(i==count){
      signal(prosfish);
    }
  }

}

void consumer_bb(int id, int count) {
  // TODO: implement the following:
  // - Iterate from 0 to count (count excluding)
  //   - read the next available value from the global array `arr_q`
  //   - print consumer id (starting from 0) and read value as:
  //     "name : consumer_X, read : X"
  int32 i;
  for(i=0;i<count;i++){
    printf("name : consumer_%d, read : %d\n",id,*tail);
    tail++;
  }
  signal(forconsbb);
}