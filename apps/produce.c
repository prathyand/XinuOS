#include <xinu.h>
#include <prodcons.h>
#include <prodcons_bb.h>
int n;
int32 arr_q[5];
int32 *head;
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
  int32 i;
  for(i=0;i<count;i++){
    printf("name : producer_%d, write : %d\n",id,*tail);
    *head=i;
    head++;
  }
  signal(forprodbb);
}
