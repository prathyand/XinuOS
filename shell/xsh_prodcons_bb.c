
#include <xinu.h>
#include <prodcons_bb.h>
#include <prodcons.h>
#include <stdio.h>
#include <shprototypes.h>
int maxcons;
shellcmd xsh_prodcons_bb(int nargs, char *args[]) {
    int arr_q[5];
    int head=0;
    int tail=0;
    mutex=semcreate(1);
    mutex2=semcreate(0);
    finallydone=semcreate(0);
    if(nargs!=5){
        printf("Syntax: run prodcons_bb <# of producer processes> <# of consumer processes> <# of iterations the producer runs> <# of iterations the consumer runs>\n");
        signal(spawnrun);
        return (0);
    }
    // printf("%d ,%d,%d,%d\n",atoi(args[1]),atoi(args[3]),atoi(args[2]),atoi(args[4]));
    if(atoi(args[1])*atoi(args[3])!=atoi(args[2])*atoi(args[4])){
        printf("Iteration Mismatch Error: the number of producer(s) iteration does not match the consumer(s) iteration\n");
        signal(spawnrun);
        return (0);
    }
    int ldp,ldc;
    // printf("fdffdf %d\n",atoi(args[2])*atoi(args[4]));
    maxcons=atoi(args[2])*atoi(args[4]);
    for(ldp=0;ldp<atoi(args[1]);ldp++){  
        resume(create(producer_bb, 1024, 20, "producer_bb", 2,ldp,atoi(args[3])));  
    }
    for(ldc=0;ldc<atoi(args[2]);ldc++){
        resume(create(consumer_bb, 1024, 20, "consumer_bb",2, ldc,atoi(args[4])));
    }
wait(finallydone);
signal(spawnrun);
return OK;
}
