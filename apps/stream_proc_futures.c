#include <xinu.h>
#include <stdlib.h>
#include "tscdf.h"
#include <future.h>
#include <runcmd.h>
sid32 spawnrun;

typedef struct data_element {
        int32 time;
        int32 value;
    }
    de;

int32 num_streams,work_queue_depth,time_window,output_time,*msgQ;

// function to dispose the msg in the port being deleted 
int disposemsg_futures(int32 portmsg){
    // used for ptdelete()
    portmsg=0;
    return 1;
}

void stream_consumer_future(int32 id, future_t *f) {
    // TODO: Consume all values from the work queue of the corresponding stream
    int procid=getpid();
    kprintf("stream_consumer id:%d (pid:%d)\n",id,procid);
    struct tscdf* tcpt= tscdf_init(time_window);
    int32 countime=0;
    // TODO: Consume all values from the work queue of the corresponding stream
    while(1==1){
        struct data_element * elem = (struct data_element *)getmem(sizeof(de));
        // wait(str->items);
        // wait(str->mutex);
        future_get(f,(char*)elem);
        
        int32 timeVar,valVar;
        timeVar=elem->time;
        valVar=elem->value;
        countime++;

        if(timeVar==0 && valVar==0){
            // signal(str->mutex);
            // signal(str->spaces);
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

        freemem((char*)elem,sizeof(de));
        // str->tail++;
        // if(str->tail==work_queue_depth){
        // str->tail=0; 
        // }
        // signal(str->mutex);
        // signal(str->spaces);

    }
    tscdf_free(tcpt);
    // semdelete(str->spaces);
    // semdelete(str->items);
    // semdelete(str->mutex);
    // freemem(str->queue,work_queue_depth*sizeof(struct data_element));
    // freemem(str,sizeof(struct stream));
    future_free(f);
    kprintf("stream_consumer exiting\n");
    ptsend(msgQ[id],procid);
}

int stream_proc_futures(int nargs, char* args[]) {

  // TODO: Parse arguments
  char usage[] = "Usage: run tscdf_fq -s <num_streams> -w <work_queue_depth> -t <time_window> -o <output_time>\n";
    int i;
    char *ch, c;
    if (nargs != 9) {
    printf("%s", usage);
    signal(spawnrun);
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
            signal(spawnrun);
            return SYSERR;
        }

        i -= 2;
    }
    }
    
    // time calc
    ulong secs, msecs, time;
    secs = clktime;
    msecs = clkticks;


  // TODO: Create futures
    future_t **futures = (future_t**)getmem(sizeof(future_t*)*num_streams);

  // TODO: Allocate futures and create consumer processes
  // Use `i` as the stream id.
    msgQ = (int32 *)getmem(num_streams*sizeof(int32));
 

    for (i = 0; i < num_streams; i++) {
        msgQ[i]=ptcreate(1);
        futures[i] = future_alloc(FUTURE_QUEUE, sizeof(struct data_element), work_queue_depth);
        resume(create(stream_consumer_future, 2048, 20, "stream_consumer_future", 2, i,futures[i]));
    }

  // TODO: Parse input header file data and populate work queue
    char* a;
    int st,ts,v,j;
    for(j=0;j<n_input;j++){
        a = (char *) stream_input[j];
        st = atoi(a);
        while (*a++ != '\t');
        ts = atoi(a);
        while (*a++ != '\t');
        v = atoi(a);
        
        struct data_element * elem = (struct data_element *)getmem(sizeof(de));
        elem->time=ts;
        elem->value=v;

        future_set(futures[st],(char*)elem);
        freemem((char*)elem,sizeof(de));
        // strms[st]->queue[strms[st]->head].time=ts;
        // strms[st]->queue[strms[st]->head].value=v;
        // strms[st]->head++;
        // if(strms[st]->head==work_queue_depth){
        //     strms[st]->head=0;
        // }

        // signal(strms[st]->mutex);
        // signal(strms[st]->items);
        
    }
  // TODO: Join all launched consumer processes
    for (i = 0; i < num_streams; i++) {
        int peid=ptrecv(msgQ[i]);
        ptdelete(msgQ[i],*disposemsg_futures);
        kprintf("process %d exited\n",peid);
    }

  // TODO: Free all futures
    freemem((char*)futures,sizeof(future_t*)*num_streams);
    freemem((char*)msgQ,num_streams*sizeof(int32));
  // TODO: Measure the time of this entire function and report it at the end
    time = (((clktime * 1000) + clkticks) - ((secs * 1000) + msecs));
    kprintf("time in ms: %u\n", time);
    signal(spawnrun);
    return 0;
}
