#include<xinu.h>
#include <future.h>

future_t* future_alloc(future_mode_t mode, uint size, uint nelem) {
  intmask mask;
  mask = disable();
 // TODO: write your code here  
  future_t* fut_mem;
  fut_mem = (future_t *)getmem(sizeof(future_t));
  fut_mem->size = size,fut_mem->mode = mode,fut_mem->state = FUTURE_EMPTY,fut_mem->data = sizeof(future_t);
  restore(mask);
  return fut_mem;
}

// TODO: write your code here for future_free, future_get and future_set
syscall future_free(future_t* f){
    intmask mask;
    mask = disable();
    f->state = FUTURE_EMPTY;
    if(f->pid !=NULL){
        kill(f->pid);
    }
    else{
        return SYSERR;
    }
    freemem(f,sizeof(future_t)+f->size);
    restore(mask);
    return OK;
}

syscall future_get(future_t* f,  char* out){
    intmask mask;
    mask = disable();
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
        memcpy(out, f->data, sizeof(out));
        f->state = FUTURE_EMPTY;
    }
    restore(mask);
    return OK;
}

syscall future_set(future_t* f, char* in){
    intmask mask;
    mask = disable();
    if(f->state==FUTURE_EMPTY){
        memcpy(f->data, in, sizeof(in));    
        f->state = FUTURE_READY;
    }
    else if(f->state==FUTURE_WAITING){
        memcpy(f->data, in, sizeof(in));
            f->state= FUTURE_EMPTY;
            resume(f->pid);
    }
    
    else{
            restore(mask);
            return SYSERR;
    }
    restore(mask);
    return OK;
    
    
}