#include <xinu.h>
#include <prodcons.h>
#include <prodcons_bb.h>
int n;
int maxcons;
int arr_q[5];
int head;
void producer(int count) {
  // TODO: implement the following:
  // - Iterates from 0 to count (count including)
  //   - setting the value of the global variable 'n' each time
  //   - print produced value (new value of 'n'), e.g.: "produced : 8"
  int i;
  for(i=0;i<=count;i++){
    wait(cons);
    n=i;
    printf("produced : %d\n",i);
    signal(prod);
  }
}

void producer_bb(int id, int count) {
  // TODO: implement the following:
  // - Iterate from 0 to count (count excluding)
  //   - add iteration value to the global array `arr_q`
  //   - print producer id (starting from 0) and written value as:
  //     "name : producer_X, write : X"
  int i;
  for(i=0;i<count;i++){
    // printf("waiting prod\n");
    wait(mutex);
    arr_q[head]=i;
    printf("name : producer_%d, write : %d\n",id,arr_q[head]);
    head++;
    if(head==5){
      head=0;
    }
    signal(mutex);
    signal(mutex2);
    // printf("done prod\n");
  }
  
  return OK;
}
