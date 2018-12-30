#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_CMD_ARG	10
#define MAX_CMD_GRP	10
#define MAX_DIR_LEN	200

const char *prompt = "myshell> ";
char* cmdgrps[MAX_CMD_GRP];
char* cmdvector[MAX_CMD_ARG];
char* cmdpipe[MAX_CMD_ARG];
char cmdline[BUFSIZ];
char dirPath[MAX_DIR_LEN];

void fatal(char*);
int makelist(char*, const char*, char**, int);

void execute_cmdline(char *cmd);
void execute_cmdgrp(char *cmdgrp);
void cdExec(char * cmdvector);
void exitExec();
void catchSignal();
int handle(char *com1[], char *com2, char dir, int in, int out, int isPiper);

int main(int argc, char**argv){
	int i=0;
	pid_t pid;	
	signal(SIGINT, SIG_IGN);
   	signal(SIGQUIT, SIG_IGN);
   	signal(SIGTSTP, SIG_IGN);
   	
	setpgid(0,0);
      
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	act.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &act, NULL);


	while (1) {
		
		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		
		cmdline[ strlen(cmdline) -1] ='\0';
		
		execute_cmdline(cmdline);

	}
	return 0;
}

void fatal(char *str){
	perror(str);

	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){	
	int numtokens = 0;
	char *snew = NULL;

	if( (s==NULL) || (delimiters==NULL) ) return -1;

	snew = s + strspn(s, delimiters);	/* delimiters skip */
	if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
		return numtokens;
	
	numtokens = 1;
	
	while(1){
	if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
		break;
		if(numtokens == (MAX_LIST-1)) return -1;
	 		numtokens++;
	}
	return numtokens;
}


int isPipe = 0;
int isLast = 0;
void execute_cmdline(char *cmdline){
	isPipe = 0;
	isLast = 0;
	int count = 0;
	int count_v =0;
	int count_p =0;
	int i = 0, j = 0, p = 0, k = 0;
	int isBackgnd = 0;
	int fd[2];
	isPipe = 0; isLast = 0;
	count = makelist(cmdline, ";", cmdgrps,MAX_CMD_GRP);

	for(i=0; i<count; ++i){
		int in = 0;
		count_p = makelist(cmdgrps[i], "|", cmdpipe, MAX_CMD_GRP);
		if (count_p > 1) isPipe = 1;
		
		for(j = 0; j < count_p; j++) {
			int checkRedir = 0;
			if (count_p - 1 == j) isLast = 1;
			count_v = makelist(cmdpipe[j], " ", cmdvector, MAX_CMD_ARG);
		

			
			char* arr[BUFSIZ];
			p = 0;
			for (k = 0; k < count_v; k++) {
				if (!strcmp(cmdvector[k], "<") || !strcmp(cmdvector[k], ">")) {
					checkRedir = 1;
					pipe(fd);
					if (isPipe) {
						handle(arr, cmdvector[k+1], cmdvector[k][0], in, fd[1], 0);
						in = fd[0];
						close(fd[1]);
					}
					else handle(arr, cmdvector[k+1], cmdvector[k][0], 0, 1, 0);
					//close(fd[1]); close(fd[0]);
					memset(arr, 0, 512); p = 0;						
				}
				else arr[p++] = cmdvector[k];		
	
			}
									
			if(!strcmp(cmdvector[count_v-1],"&")){
				cmdvector[count_v -1] = '\0';
				isBackgnd = 1;
			}
            
			if(!strcmp(cmdvector[0],"cd")) 
			{
				cdExec(cmdvector[1]);
			}
            
			else if (!strcmp(cmdvector[0],"exit"))
			{
				exitExec();
			}
			else if (isPipe == 0 && checkRedir == 0){
				pid_t pid = fork();
				switch(pid){
					case -1:	fatal("fork error\n");
					case 0:	
					      signal(SIGINT, SIG_DFL);
					      signal(SIGQUIT, SIG_DFL);
					      signal(SIGTSTP, SIG_DFL);
					      execute_cmdgrp(cmdpipe[j]);
					default : 
					if (isBackgnd == 0)
						waitpid(pid,NULL,0);
					fflush(stdout); 
				}
			}
			else if (isPipe == 1 && checkRedir == 0){
				pipe(fd);
		
				if (isLast) 
					handle(cmdvector, ".", '.', in, 1, 1);
			
				handle(cmdvector, ".", '.', in, fd[1], 1);
				close(fd[1]);
			
				in = fd[0];
			

			}


		}
	}
}

int handle(char *com1[], char *com2, char dir, int in, int out, int isPiper) {
	int fd, fd2, redir = 0;
	char cat[5] = "cat";
	if (!isPiper) {
		
		if (dir == '>') redir = 1;
		if((fd = open(com2, O_RDWR)) == -1) 
			fatal(com2);
		fd2 = open(com1[0], O_RDWR);
	}
	
	switch(fork()) {
	case -1: fatal("2nd fork call in join_redir");
	case 0:
		if(in != 0) {
			dup2(in, 0);
			close(in);
		}
		if(out != 1) {
			dup2(out, 1);
			close(out);
		}
		if(!isPiper) {
			
			dup2(fd, redir);
			if (fd2 > 0) return execlp(cat, cat, com1[0], (char*)NULL);
			else return execvp(com1[0], com1);
			fatal("1st execvp call in join_redir");
		} else {
			
			return execvp(com1[0], com1);

		}
	default:
		close(fd2);		
            return 0;
	}

}


void execute_cmdgrp(char *cmdgrp){
	int i=0;
	int count = 0;
	
	execvp(cmdvector[0], cmdvector);

	fatal("exec error\n");

}
// Handles ChangeDirectory Command
void cdExec(char * cmdvector){
	chdir(cmdvector);
	printf("%s\n",getcwd(dirPath,MAX_DIR_LEN));
	return;
}

//Handles Exit command
void exitExec(){
	exit(0);
}
