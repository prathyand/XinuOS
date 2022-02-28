#include <xinu.h>
#include <stdio.h>
#include <shprototypes.h>
#include <prodcons.h>
#include <future_prodcons.h>
#include <future.h>
#include <runcmd.h>

sid32 spawnrun;
 int future_fib(int nargs, char *args[]);
 int future_free_test(int nargs, char *args[]);

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
  if(nargs<3){
    printf("Syntax: run futest [-pc [g ...] [s VALUE ...]|-f NUMBER][--free]");
    return 1;
  }

  if(strncmp(args[1], "--pc" ,  4)==0){
    if(nargs<=3){
      printf("Syntax: run futest [-pc [g ...] [s VALUE ...]|-f]\n");
      return 1;
    }
    else {
      future_prodcons(nargs, args);
      return 0;
    }
  }

  if(strncmp(args[1], "--f" ,  3)==0){
    future_fib(nargs, args);
    return 0;
  }
  if(strncmp(args[1], "--free" ,  6)==0){
    future_free_test(nargs, args);
    return 0;
  }
  
}

for(i=0;i<funclength;i++){
        printf("%s\n",funcs[i]);
    }  

}