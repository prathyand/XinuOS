#include<xinu.h>
#include <future.h>

future_t* future_alloc(future_mode_t mode, uint size, uint nelem) {
  intmask mask;
  mask = disable();
 // TODO: write your code here  
  future_t* fut_mem;
  char* futdata = getmem(size);
  fut_mem = (future_t*)getmem(sizeof(future_t));
  fut_mem->size = size,fut_mem->mode = mode,fut_mem->state = FUTURE_EMPTY,fut_mem->data = futdata;

  if(mode==FUTURE_SHARED){
    fut_mem->get_queue = newqueue();  
  }

  restore(mask);
  return fut_mem;
}

// TODO: write your code here for future_free, future_get and future_set
syscall future_free(future_t* f){
    intmask mask;
    mask = disable();
    // this is for SHARED mode
    if(f->mode==FUTURE_SHARED){ 
    f->state = FUTURE_EMPTY;
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

    // this is for EXCLUSIVE mode
    if(f->mode==FUTURE_EXCLUSIVE){ 
    f->state = FUTURE_EMPTY;
    if(f->pid !=NULL){
        kill(f->pid);
    }
    
    freemem(f->data,f->size);
    freemem(f,sizeof(future_t));
    restore(mask);
    return OK;
    }
    // If mode is neither exclusive nor shared
    restore(mask);
    return SYSERR;
}

syscall future_get(future_t* f,  char* out){
    intmask mask;
    mask = disable();
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

    // this is for exclusive mode
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

    // If mode is neither exclusive nor shared
    restore(mask);
    return SYSERR;
}

syscall future_set(future_t* f, char* in){
    intmask mask;
    mask = disable();
    // this is for exclusive mode
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
    // This is for shared mode
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
    // If mode is neither exclusive nor shared
    restore(mask);
    return SYSERR;
    
}