/* xsh_hello.c - xsh_hello */

#include <xinu.h>
#include <stdio.h>
#include <string.h>
#include <prodcons.h>
#include <runcmd.h>
/*------------------------------------------------------------------------
 * xsh_hello
 *------------------------------------------------------------------------
 */
sid32 spawnrun;
shellcmd xsh_hello(int nargs, char *args[])
{
	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		printf("Use: %s\n\n", args[0]);
		printf("Description:\n");
		printf("\tWelcomes the user to the world of Xinu!\n");
		printf("Options:\n");
        printf("\tusername\t username to be displayed in the welcome dialogue\n");
		printf("\t--help\t display this help and exit\n");
		signal(spawnrun);
		return 0;
	}

	if (nargs < 2) {
		// fprintf(stderr, "too few arguments\n");	
        // fprintf(stderr, "Try '%s --help' for more information\n",args[0]);
		fprintf(stderr, "Syntax: run hello name\n");
	}
    else if (nargs > 2) {
		// fprintf(stderr, "%s:too many arguments\n",args[0]);	
        // fprintf(stderr, "Try '%s --help' for more information\n",args[0]);
		fprintf(stderr, "Syntax: run hello name\n");
	}
    else{
        printf("Hello %s, Welcome to the world of Xinu!!\n",args[1]);
    }

	//printf("\n");
	signal(spawnrun);
	return OK;
}