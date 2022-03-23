#include<xinu.h>
#include <future.h>

future_t* future_alloc(future_mode_t mode, uint size, uint nelem) {
    intmask mask;
    mask = disable();
    // TODO: write your code here  
    future_t* fut_mem;
    fut_mem = (future_t*)getmem(sizeof(future_t));

    if(mode!=FUTURE_QUEUE){
        fut_mem->data = (char*)getmem(size);
    }else{
        fut_mem->data = (char*)getmem(size*nelem);
    }
    
    fut_mem->size = size,fut_mem->mode = mode,fut_mem->state = FUTURE_EMPTY;
    if(mode==FUTURE_SHARED || mode==FUTURE_QUEUE){
        fut_mem->get_queue = newqueue(); 
        if(mode==FUTURE_QUEUE){
            fut_mem->set_queue = newqueue(); 
            fut_mem->max_elems=nelem;
            fut_mem->count=0;
            fut_mem->head=0;
            fut_mem->tail=0;
            // kprintf("future created maxElem:%d\n",fut_mem->max_elems);
        } 
    }
    restore(mask);
    return fut_mem;
}

// TODO: write your code here for future_free, future_get and future_set
syscall future_free(future_t* f){
    intmask mask;
    mask = disable();
    
    if(f->mode!=FUTURE_SHARED && f->mode!=FUTURE_EXCLUSIVE && f->mode!=FUTURE_QUEUE){
        restore(mask);
        return SYSERR;
    }

    if(f->pid !=NULL){
        kill(f->pid);
    }
    // for FUTURE_EXCLUSIVE
    if(f->mode==FUTURE_EXCLUSIVE){
        freemem(f->data,f->size);
    }
    if(f->mode==FUTURE_SHARED){ 
        pid32 ep;
        while(TRUE){
            ep = dequeue(f->get_queue);
            if(ep==EMPTY){
                break;
            }   
            kill(ep);
        }
        delqueue(f->get_queue);
        freemem(f->data,f->size);
        freemem(f,sizeof(future_t));
        restore(mask);
        return OK;
    }

    if(f->mode==FUTURE_QUEUE){ 
        pid32 ep1,ep2;
        while(TRUE){
            ep1 = dequeue(f->get_queue);
            ep2 = dequeue(f->set_queue);
            if(ep1!=EMPTY){
                kill(ep1);
            }   
            if(ep2!=EMPTY){
                kill(ep2);
            }  
            if(ep1==EMPTY && ep2==EMPTY){
                break;
            }
        }
        delqueue(f->get_queue);
        delqueue(f->set_queue);
        freemem(f->data,f->size*f->max_elems);
        freemem(f,sizeof(future_t));
        restore(mask);
        return OK;
    }
  
    freemem(f,sizeof(future_t));
    restore(mask);
    return SYSERR;
}

syscall future_get(future_t* f,  char* out){
    intmask mask;
    mask = disable();
    if(f->mode==FUTURE_QUEUE){
        
        // check the count 
        if(f->count==0){
            pid32 gpid=getpid();
            enqueue(gpid,f->get_queue);
            // kprintf("in futget c=0, suspended and enqued pid %d\n",gpid);
            suspend(gpid);
            // restore(mask);
            // return OK;
        }
        
        char* headelemptr = f->data + (f->head * f->size);
        // kprintf("reading %d from head=%d %d\n",*headelemptr,f->head,headelemptr);
        memcpy(out,headelemptr,f->size);
        f->head+=1;
        if(f->head==f->max_elems){
            // kprintf("resetting head to 0 FG\n");
            f->head=0;
        }
        f->count-=1;
        // Now we resume 1 setter if set_queue is not empty
        pid32 setprosid = dequeue(f->set_queue);
        if(setprosid!=EMPTY){
            resume(setprosid);
        }
        restore(mask);
        return OK;
    }

    // this is for SHARED mode
    if(f->mode==FUTURE_SHARED){
        if(f->state==FUTURE_EMPTY || f->state==FUTURE_WAITING){
            f->state = FUTURE_WAITING;
            f->pid = getpid();
            suspend(f->pid);
            enqueue(f->pid,f->get_queue);
            memcpy(out, f->data, sizeof(out));
        }
        else{
            memcpy(out, f->data, f->size);
        }
        restore(mask);
        return OK;
    }

    // this is for EXCLUSIVE mode
    if(f->mode==FUTURE_EXCLUSIVE){ 
        if(f->state==FUTURE_EMPTY){
            f->state = FUTURE_WAITING;
            f->pid = getpid();
            suspend(f->pid);
            memcpy(out, f->data, sizeof(out));
            f->state = FUTURE_EMPTY;
        }
        
        else if(f->state==FUTURE_WAITING){
            restore(mask);
            return SYSERR;
        }
        else{
            memcpy(out, f->data, f->size);
            f->state = FUTURE_EMPTY;
        }
        restore(mask);
        return OK;
    }
    // If mode is neither EXCLUSIVE nor SHARED
    restore(mask);
    return SYSERR;
}

syscall future_set(future_t* f, char* in){
    intmask mask;
    mask = disable();
    if(f->mode==FUTURE_QUEUE){
        // check if the queue is full
        if(f->count==f->max_elems){
            pid32 gpid=getpid();
            enqueue(gpid,f->set_queue);
            suspend(gpid);
            // restore(mask);
            // return OK;
        }

        char* tailelemptr = f->data + (f->tail * f->size);
        // kprintf("writing %d to tail=%d,%d\n",*in,f->tail,tailelemptr);
        memcpy(tailelemptr, in, f->size);
        f->tail+=1;
        if(f->tail==f->max_elems){
            // kprintf("resetting head to 0 FS me%d\n");
            f->tail=0;
        }
        f->count+=1;
        // Now we resume 1 getter if get_queue is not empty
        pid32 getprosid = dequeue(f->get_queue);
        if(getprosid!=EMPTY){
            // kprintf("resuming waiting getter\n");
            resume(getprosid);
        }
        restore(mask);
        return OK;
    }

    // ------this is for EXCLUSIVE mode---------
    if(f->mode==FUTURE_EXCLUSIVE){ 
        if(f->state==FUTURE_EMPTY){
            memcpy(f->data, in, f->size);    
            f->state = FUTURE_READY;
        }
        else if(f->state==FUTURE_WAITING){  
            memcpy(f->data, in, f->size);
            f->state= FUTURE_READY;
            resume(f->pid);
        }
    
        else{
            restore(mask);
            return SYSERR;
        }
        restore(mask);
        return OK;
    }
    // This is for SHARED mode
    else if(f->mode==FUTURE_SHARED){

        if(f->state==FUTURE_EMPTY || f->state==FUTURE_WAITING){
            memcpy(f->data, in, f->size);    
            f->state = FUTURE_READY;
            pid32 ep;
            while(TRUE){
                ep = dequeue(f->get_queue);
                if(ep==EMPTY){
                    break;
                }   
                resume(ep);
            }
        }
        else{
                restore(mask);
                return SYSERR;
        }
        restore(mask);
        return OK;
    }
    // If mode is neither EXCLUSIVE nor SHARED
    restore(mask);
    return SYSERR;
    
}