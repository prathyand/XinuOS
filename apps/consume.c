#include <xinu.h>
#include <prodcons.h>
#include <prodcons_bb.h>
#include <runcmd.h>
int n;
int maxcons;
int arr_q[5];
int tail;
sid32 spawnrun;
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
    // printf("waiting cons\n");
    wait(mutex2);
    wait(mutex);
    printf("name : consumer_%d, read : %d\n",id,arr_q[tail]);
    // printf("maxcons %d\n",maxcons);
    tail++;
    maxcons--;
    if(tail==5){
      tail=0;
    }
    signal(mutex);
    // printf("done cons\n");
  }
  if(maxcons==0){
    // printf("reached here\n");
    signal(finallydone);
  }
  // printf("tail and last arr %d,%d\n",tail,&arr_q[maxcons]);
  // if(tail==&arr_q[maxcons]){
  //   signal(finallydone);
  // }
 return OK;
}