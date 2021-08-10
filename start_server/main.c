//
//  main.c
//  start_server
//
//  Created by Vincent Moscaritolo on 8/6/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>


extern char **environ;

int main(int argc, const char * argv[]) {

	pid_t pid;
	int status;
  
  char* path = "./insteonserver";
  char *args[] = {/*"InsteonMgr",*/ "-f /home/vinthewrench/projects/InsteonServer/InsteonDB.txt",(char *) 0};

  status = posix_spawn(&pid,path ,NULL,NULL,args,environ);
	if (status == 0) {
		 printf("Child pid: %i\n", pid);

	} else {
		 printf("posix_spawn: %s\n", strerror(status));
	}
	return 0;
}
