#include <xinu.h>
#include <stdio.h>
#include <shprototypes.h>
#include <prodcons.h>
#include <future_prodcons.h>
#include <future.h>

void future_prodcons(int nargs, char *args[]) {

  print_sem = semcreate(1);
  future_t* f_exclusive;
  f_exclusive = future_alloc(FUTURE_EXCLUSIVE, sizeof(int), 1);
  char *val;
  int i;
  i=2;
  while (i < nargs) {
        if ((strcmp(args[i], "s") == 0) || (atoi(args[i])!=0) || (strcmp(args[i], "g") == 0) ){
          i++;
        }  
        else{               
        printf("Syntax: run futest [-pc [g ...] [s VALUE ...]|-f]\n");
        signal(spawnrun);
        return 1;   
        }                 
    
  }

  int num_args = i;  
  i = 2; 
  val  =  (char *) getmem(num_args); 

  // Iterate again through the arguments and create the following processes based on the passed argument ("g" or "s VALUE")
  while (i < nargs) {
    if (strcmp(args[i], "g") == 0) {
      char id[10];
      sprintf(id, "fcons%d",i);
      resume(create(future_cons, 2048, 20, id, 1, f_exclusive));
    }
    if (strcmp(args[i], "s") == 0) {
      i++;
      uint8 number = atoi(args[i]);
      val[i] = number;
      resume(create(future_prod, 2048, 20, "fprod1", 2, f_exclusive, &val[i]));
      sleepms(5);
    }
    i++;
  }
  sleepms(100);
  future_free(f_exclusive);
  signal(spawnrun);
  return OK;
}


shellcmd xsh_run(int nargs, char *args[]) {
char *funcs[50]={"hello","list","prodcons","prodcons_bb","futest"};
int funclength,i;
i=0;

spawnrun=semcreate(0);

while(funcs[i] && i<50){
i+=1;
}
funclength=i;

if((nargs==1) || (strncmp(args[1], "list", 4) == 0)){
    for(i=0;i<funclength;i++){
        printf("%s\n",funcs[i]);
    }
    return OK;
}

args++;
nargs--;

if(strncmp(args[0], "hello", 5) == 0) {
  /* create a process with the function as an entry point. */
  resume (create((void *) xsh_hello, 4096, 20, "hello", 2, nargs, args));
  wait(spawnrun);
  return OK;
}

if(strncmp(args[0], "prodcons_bb", 11) == 0) {
  /* create a process with the function as an entry point. */
  resume (create((void *) xsh_prodcons_bb, 4096, 20, "prodcons_bb", 2, nargs, args));
  wait(spawnrun);
  return OK;
}

if(strncmp(args[0], "prodcons", 8) == 0) {
  /* create a process with the function as an entry point. */
  resume (create((void *) xsh_prodcons, 4096, 20, "prodcons", 2, nargs, args));
  wait(spawnrun);
  return OK;
}
if(strncmp(args[0], "futest" ,  6)==0){
        if(nargs<=3){
            printf("Syntax: run futest [-pc [g ...] [s VALUE ...]|-f]\n");
            return 0;
        }
        else {
            resume(create(future_prodcons, 4096, 20, "future_prodcons", 2,nargs, args));
            wait(spawnrun);
            return 1;
        }
    }
for(i=0;i<funclength;i++){
        printf("%s\n",funcs[i]);
    }
    

}