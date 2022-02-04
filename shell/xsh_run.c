#include <xinu.h>
#include <stdio.h>
#include <shprototypes.h>

shellcmd xsh_run(int nargs, char *args[]) {
char *funcs[50]={"hello","list","prodcons"};
int funclength,i;
i=0;
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
  return OK;
}

if(strncmp(args[0], "prodcons", 8) == 0) {
  /* create a process with the function as an entry point. */
  resume (create((void *) xsh_prodcons, 4096, 20, "prodcons", 2, nargs, args));
  return OK;
}

for(i=0;i<funclength;i++){
        printf("%s\n",funcs[i]);
    }

}