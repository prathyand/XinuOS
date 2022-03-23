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
int32 num_streams,work_queue_depth,time_window,output_time,*msgQ;

// function to disopse the msg in the port being deleted 
void disposemsg(int32 portmsg){
    // used for ptdelete()
    portmsg=0;
}

void stream_consumer(int32 id, struct stream * str) {
    int procid=getpid();
    kprintf("stream_consumer id:%d (pid:%d)\n",id,procid);
    struct tscdf* tcpt= tscdf_init(time_window);
    int32 countime=0;
    // TODO: Consume all values from the work queue of the corresponding stream
    while(1==1){
        wait(str->items);
        wait(str->mutex);
        
        int32 timeVar,valVar;
        timeVar=str->queue[str->tail].time;
        valVar=str->queue[str->tail].value;
        countime++;

        if(timeVar==0 && valVar==0){
            signal(str->mutex);
            signal(str->spaces);
            break;
        }

        tscdf_update(tcpt,timeVar,valVar);

        if(countime%output_time==0){ 
            int32 *qarray = tscdf_quartiles(tcpt);
            if (qarray == NULL) {
            kprintf("tscdf_quartiles returned NULL\n");
            continue;
            }

            kprintf("s%d: %d %d %d %d %d\n", id, qarray[0], qarray[1], qarray[2], qarray[3], qarray[4]);
            freemem((char *) qarray, (6*sizeof(int32)));
        }

        str->tail++;
        if(str->tail==work_queue_depth){
        str->tail=0; 
        }
        signal(str->mutex);
        signal(str->spaces);

    }

    //delete semaphores and release memory from the exiting stream
    tscdf_free(tcpt);
    semdelete(str->spaces);
    semdelete(str->items);
    semdelete(str->mutex);
    freemem(str->queue,work_queue_depth*sizeof(struct data_element));
    freemem(str,sizeof(struct stream));

    kprintf("stream_consumer exiting\n");
    ptsend(msgQ[id],procid);
}

int stream_proc(int nargs, char * args[]) {
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
    
    // time calc
    ulong secs, msecs, time;
    secs = clktime;
    msecs = clkticks;

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

        wait(strms[st]->spaces);
        wait(strms[st]->mutex);

        strms[st]->queue[strms[st]->head].time=ts;
        strms[st]->queue[strms[st]->head].value=v;
        strms[st]->head++;
        if(strms[st]->head==work_queue_depth){
            strms[st]->head=0;
        }

        signal(strms[st]->mutex);
        signal(strms[st]->items);
        
        
    }
    // TODO: Join all launched consumer processes
    for (i = 0; i < num_streams; i++) {
        int peid=ptrecv(msgQ[i]);
        ptdelete(msgQ[i],*disposemsg);
        kprintf("process %d exited\n",peid);
    }
    
    // TODO: Measure the time of this entire function and report it at the end
    time = (((clktime * 1000) + clkticks) - ((secs * 1000) + msecs));
    kprintf("time in ms: %u\n", time);

    // release memory
    freemem(strms,num_streams*sizeof(struct stream*));
    freemem(msgQ,num_streams*sizeof(int32));

    signal(spawnrun);
    return OK;
}