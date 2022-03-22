#include <xinu.h>
#include "tscdf.h"
#include "tscdf_input.h"
#include <runcmd.h>
sid32 spawnrun;

typedef struct data_element {
        int32 time;
        int32 value;
    }
    de;

    struct stream {
        sid32 spaces;
        sid32 items;
        sid32 mutex;
        int32 head;
        int32 tail;
        struct data_element * queue;
    };
int num_streams,work_queue_depth,time_window,output_time;
int32 processexitcount=0;
// sid32 allconsexited;
int32 *msgQ;

void stream_consumer(int32 id, struct stream * str) {
    int procid=getpid();
    kprintf("stream_consumer id:%d (pid:%d)\n",id,procid);
    struct tscdf* tcpt= tscdf_init(time_window);
    int32 countime=0;
    // TODO: Consume all values from the work queue of the corresponding stream
    while(1==1){
        // kprintf("C-Li: %d,%d <S%d I%d M%d>\n",id,procid,semcount(str->spaces),semcount(str->items),semcount(str->mutex));
        wait(str->items);
        // kprintf("C-Lm: %d,%d <S%d I%d M%d>\n",id,procid,semcount(str->spaces),semcount(str->items),semcount(str->mutex));
        wait(str->mutex);
        
        int32 timeVar,valVar;
        timeVar=str->queue[str->tail].time;
        valVar=str->queue[str->tail].value;
        // kprintf("time val at C %d %d %d\n",id,timeVar,valVar);
        
        
        countime++;
        char* output;
        
        tscdf_update(tcpt,timeVar,valVar);

        if(countime%output_time==0){ 
            int32 *qarray = tscdf_quartiles(tcpt);

            if (qarray == NULL) {
            kprintf("tscdf_quartiles returned NULL\n");
            break;
            }

            kprintf("s%d: %d %d %d %d %d\n", id, qarray[0], qarray[1], qarray[2], qarray[3], qarray[4]);
            // kprintf("%s\n", output);
            freemem((char *) qarray, (6*sizeof(int32)));
        }

        str->tail++;
        if(str->tail==work_queue_depth){
        str->tail=0; 
        }
        // kprintf("C-Rm: %d,%d <S%d I%d M%d>\n",id,procid,semcount(str->spaces),semcount(str->items),semcount(str->mutex));
        signal(str->mutex);
        // kprintf("C-Rs: %d,%d <S%d I%d M%d>\n",id,procid,semcount(str->spaces),semcount(str->items),semcount(str->mutex));
        signal(str->spaces);
        
        if(timeVar==0 && valVar==0){
            break;
        }
        
        
        
    }

    tscdf_free(tcpt);

    
    freemem(str->queue,work_queue_depth*sizeof(struct data_element));
    freemem(str,sizeof(struct stream));
    processexitcount++;
    kprintf("stream_consumer exiting\n");
    
    
    ptsend(msgQ[id],procid);
    
    if(processexitcount==num_streams){
        // signal(allconsexited);
    }
    return OK;

}

int stream_proc(int nargs, char * args[]) {
    
    // time calc
    ulong secs, msecs, time;
    secs = clktime;
    msecs = clkticks;
    // allconsexited = semcreate(0);
    // TODO: Parse arguments
    char usage[] = "Usage: run tscdf -s <num_streams> -w <work_queue_depth> -t <time_window> -o <output_time>\n";

    int i;
    char *ch, c;
    if (nargs != 9) {
    printf("%s", usage);
    return SYSERR;
    } else {
    i = nargs - 1;
    while (i > 0) {
        ch = args[i - 1];
        c = *(++ch);

        switch (c) {
        case 's':
            num_streams = atoi(args[i]);
            break;

        case 'w':
            work_queue_depth = atoi(args[i]);
            break;

        case 't':
            time_window = atoi(args[i]);
            break;

        case 'o':
            output_time = atoi(args[i]);
            break;

        default:
            printf("%s", usage);
            return SYSERR;
        }

        i -= 2;
    }
    }
    
    // TODO: Create streams
    struct stream **strms = (struct stream**)getmem(sizeof(struct stream*)*num_streams);
    for (i=0;i<num_streams;i++){
        strms[i] = (struct stream*)getmem(sizeof(struct stream));
    }

    msgQ = (int32 *)getmem(num_streams*sizeof(int32));


    for (i = 0; i < num_streams; i++) {
        msgQ[i]=ptcreate(1);
    }
    
    
    // TODO: Create consumer processes and initialize streams
    // Use `i` as the stream id.
    for (i = 0; i < num_streams; i++) {

        strms[i]->queue = (struct data_element*)getmem(work_queue_depth*sizeof(struct data_element));
        strms[i]->head=0;
        strms[i]->tail=0;
        strms[i]->spaces=semcreate(work_queue_depth);
        strms[i]->items=semcreate(0);
        strms[i]->mutex=semcreate(1);
        resume(create((void *) stream_consumer, 1024, 20, "stream_consumer", 2, i,strms[i]));
    }


    // TODO: Parse input header file data and populate work queue
    char* a;
    int st,ts,v,j;
    for(j=0;j<number_inputs;j++){
        
        a = (char *) stream_input[j];
        st = atoi(a);
        while (*a++ != '\t');
        ts = atoi(a);
        while (*a++ != '\t');
        v = atoi(a);
        // if(ts==0 && v==0){
        //     kprintf("last input for i: %d\n",st);
        // }
        // kprintf("P-Ls: %d <S%d I%d M%d>\n",st,semcount(strms[st]->spaces),semcount(strms[st]->items),semcount(strms[st]->mutex));
        wait(strms[st]->spaces);
        // kprintf("P-Lm: %d <S%d I%d M%d>\n",st,semcount(strms[st]->spaces),semcount(strms[st]->items),semcount(strms[st]->mutex));
        wait(strms[st]->mutex);
        
        // printf("strms[st]->spaces done\n");
        
        // printf("strms[st]->mutex done\n");
        strms[st]->queue[strms[st]->head].time=ts;
        strms[st]->queue[strms[st]->head].value=v;
        strms[st]->head++;
        if(strms[st]->head==work_queue_depth){
            strms[st]->head=0;
        }
        // kprintf("P-Rm: %d <S%d I%d M%d>\n",st,semcount(strms[st]->spaces),semcount(strms[st]->items),semcount(strms[st]->mutex));
        signal(strms[st]->mutex);
        // kprintf("P-Ri: %d <S%d I%d M%d>\n",st,semcount(strms[st]->spaces),semcount(strms[st]->items),semcount(strms[st]->mutex));
        signal(strms[st]->items);
        
        
    }
    // kprintf("done producing\n");
    // wait(allconsexited);

    // TODO: Join all launched consumer processes
    for (i = 0; i < num_streams; i++) {
        int peid;
        // printf("waiting for process i%d\n",i);
        peid=ptrecv(msgQ[i]);
        printf("process %d exited\n",peid);
    }
    
    // TODO: Measure the time of this entire function and report it at the end
    time = (((clktime * 1000) + clkticks) - ((secs * 1000) + msecs));
    printf("time in ms: %u\n", time);
    return OK;
}