
#include <xinu.h>
#include <prodcons_bb.h>
#include <prodcons.h>
#include <stdio.h>
#include <shprototypes.h>

shellcmd xsh_prodcons_bb(int nargs, char *args[]) {
    int arr_q[5];
    int *head=arr_q;
    int *tail=arr_q;
    forprodbb=semcreate(0);
    forconsbb=semcreate(0);
    if(nargs!=5){
        fprintf(stderr, "Syntax: run prodcons_bb <# of producer processes> <# of consumer processes> <# of iterations the producer runs> <# of iterations the consumer runs>\n");
        signal(spawnrun);
        return (0);
    }

    if(atoi(args[1])*atoi(args[3])!=atoi(args[2])*atoi(args[4])){
        fprintf(stderr, "Iteration Mismatch Error: the number of producer(s) iteration does not match the consumer(s) iteration\n");
        signal(spawnrun);
        return (0);
    }
    int ldp,ldc;

    for(ldp=0;ldp<atoi(args[1]);ldp++){
        forprodbb=semcreate(0);
        resume(create(producer_bb, 1024, 20, "producer_bb", 2,ldp,atoi(args[3])));
        wait(forprodbb);
    }
    for(ldc=0;ldc<atoi(args[2]);ldc++){
        forconsbb=semcreate(0);
        resume(create(consumer_bb, 1024, 20, "producer_bb",2, ldc,atoi(args[4])));
        wait(forconsbb);
    }
signal(spawnrun);
return OK;

}
