/* 
 * tsh - A tiny shell program with job control
 * 
 * Andrew Jones
 * akj53
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
	int c;
	char cmdline[MAXLINE];
	int emit_prompt = 1; /* emit prompt (default) */

	/* Redirect stderr to stdout (so that driver will get all output
	 * on the pipe connected to stdout) */
	dup2(1, 2);

	/* Parse the command line */
	while ((c = getopt(argc, argv, "hvp")) >= 0) {
		switch (c) {
			case 'h':             /* print help message */
				usage();
				break;
			case 'v':             /* emit additional diagnostic info */
				verbose = 1;
				break;
			case 'p':             /* don't print a prompt */
				emit_prompt = 0;  /* handy for automatic testing */
				break;
			default:
				usage();
		}
	}

	/* Execute the shell's read/eval loop */
	while (1) {

		/* Read command line */
		if (emit_prompt) {
			printf("%s", prompt);
			fflush(stdout);
		}
		if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
			app_error("fgets error");
		if (feof(stdin)) { /* End of file (ctrl-d) */
			fflush(stdout);
			exit(0);
		}

		/* Evaluate the command line */
		fflush(stdout);
		eval(cmdline);
		fflush(stdout);
	} 

	exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit) then execute it
 * immediately. Otherwise, build a pipeline of commands and wait for all of
 * them to complete before returning.
*/
void eval(char *cmdline) 
{
	// tokenize commands and redirect symbols
	char *argv[MAXARGS];
	parseline(cmdline,argv);
	// if 'quit' is passed as the command, abort
	if (builtin_cmd(argv) != 0)
		return;

	// interpret redirect instructions and separate relevant arguments
	int cmd[MAXARGS], stdin_redir[MAXARGS], stdout_redir[MAXARGS];
	int cmd_count = parseargs(argv,cmd,stdin_redir,stdout_redir);

	// spawn children for each command, equipped with pipes & redirects as necessary
	int pnum = 0;
	int pid = -1;
	int pidlist[cmd_count];
	int pgid = -1;

	int pipefd[2] = {-1,-1};
	int pipefd_old = -1;
	while (pnum < cmd_count) {
		// hold onto read-pipe from the previous command
		pipefd_old = pipefd[0];
		// initialize pipe connecting to successive child process, if applicable
		if (pnum < cmd_count-1) {
			// create pipe for subsequent command
			if (pipe(pipefd) == -1) {
				perror("eval()");
				exit(1);
			}
			//printf("pipe created: {%d,%d}\n",pipefd[0],pipefd[1]);
		}

		// spawn child[pnum]
		pid = fork();
		if (pid == -1) { // failed to spawn child
			perror("eval()");
			exit(1);
		} else if (pid == 0) { // child
			// redirect file into stdin?
			if (stdin_redir[pnum] > 0) {
				int auxin = open(argv[stdin_redir[pnum]],O_RDONLY);
				//printf("reading file '%s' [fd=%d]\n",argv[stdin_redir[pnum]],auxin);
				if (auxin == -1) {
					fprintf(stderr,"eval() [child %d] opening file %s: ",pnum,argv[stdin_redir[pnum]]);
					perror(NULL);
				}
				//printf("[child %d] duplicating fd=%d onto stdin\n",pnum,auxin);
				dup2(auxin,STDIN_FILENO);
				close(auxin);
			// redirect previous pipe read-end into stdin?
			} else if (pnum > 0) {
				//printf("[child %d] duplicating pipe %d onto stdin\n",pnum,pipefd_old);
				dup2(pipefd_old,STDIN_FILENO);
				close(pipefd_old);
			}

			// redirect stdout to file?
			if (stdout_redir[pnum] > 0) {
				int auxout = open(argv[stdout_redir[pnum]], O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
				//printf("writing to file '%s' [fd=%d]\n",argv[stdout_redir[pnum]],auxout);
				if (auxout == -1) {
					fprintf(stderr,"eval() [child %d] opening file %s: ",pnum,argv[stdout_redir[pnum]]);
					perror(NULL);
				}
				//printf("[child %d] duplicating fd=%d onto stdout\n",pnum,pipefd[1]);
				dup2(auxout,STDOUT_FILENO);
				close(auxout);
			// redirect current pipe write-end into stdout?
			} else if (pnum < cmd_count-1) {
				//printf("[child %d] duplicating pipe %d onto stdout\n",pnum,pipefd[1]);
				dup2(pipefd[1],STDOUT_FILENO);
				close(pipefd[1]);
			}

			// we have an extra pipe read-end open, intended for the successive child
			if (pnum < cmd_count-1 && close(pipefd[0]) < 0) {
				fprintf(stderr,"eval() [child %d] closing pipe: ",pnum);
				perror(NULL);
				exit(1);
			}

			execve(argv[cmd[pnum]],&argv[cmd[pnum]],environ);
			// if execution fails, print error message and return control to parent shell
			fprintf(stderr,"%s: ",argv[cmd[pnum]]);
			perror(NULL);
			exit(1);
		} else { // parent
			// close read end of previous pipe
			if (pipefd_old > 0 && close(pipefd_old) < 0) {
				perror("eval() closing old pipe read end");
			}
			// close write end of current pipe
			if (pnum < cmd_count-1 && close(pipefd[1]) < 0) {
				perror("eval() closing old pipe write end");
			}

			pidlist[pnum] = pid;
			// set groupids of all child processes to match the pid of child 0
			if (pnum == 0) {
				pgid = pid;
			}
			if (setpgid(pid, pgid) < 0){
				perror("eval() [parent]");
				exit(1);
			}
			pnum++;
		}
	}

	// wait for all processes to complete
	for (pnum=0; pnum<cmd_count; pnum++) {
		waitpid(pidlist[pnum],NULL,0);
	}
	return;
}

/* 
 * parseargs - Parse the arguments to identify pipelined commands
 * 
 * Walk through each of the arguments to find each pipelined command.  If the
 * argument was | (pipe), then the next argument starts the new command on the
 * pipeline.  If the argument was < or >, then the next argument is the file
 * from/to which stdin or stdout should be redirected, respectively.  After it
 * runs, the arrays for cmds, stdin_redir, and stdout_redir all have the same
 * number of items---which is the number of commands in the pipeline.  The cmds
 * array is populated with the indexes of argv corresponding to the start of
 * each command sequence in the pipeline.  For each slot in cmds, there is a
 * corresponding slot in stdin_redir and stdout_redir.  If the slot has a -1,
 * then there is no redirection; if it is >= 0, then the value corresponds to
 * the index in argv that holds the filename associated with the redirection.
 * 
 */
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir) 
{
	int argindex = 0;    /* the index of the current argument in the current cmd */
	int cmdindex = 0;    /* the index of the current cmd */

	if (!argv[argindex]) {
		return 0;
	}

	cmds[cmdindex] = argindex;
	stdin_redir[cmdindex] = -1;
	stdout_redir[cmdindex] = -1;
	argindex++;
	while (argv[argindex]) {
		if (strcmp(argv[argindex], "<") == 0) {
			argv[argindex] = NULL;
			argindex++;
			if (!argv[argindex]) { /* if we have reached the end, then break */
				break;
			}
			stdin_redir[cmdindex] = argindex;
		} else if (strcmp(argv[argindex], ">") == 0) {
			argv[argindex] = NULL;
			argindex++;
			if (!argv[argindex]) { /* if we have reached the end, then break */
				break;
			}
			stdout_redir[cmdindex] = argindex;
		} else if (strcmp(argv[argindex], "|") == 0) {
			argv[argindex] = NULL;
			argindex++;
			if (!argv[argindex]) { /* if we have reached the end, then break */
				break;
			}
			cmdindex++;
			cmds[cmdindex] = argindex;
			stdin_redir[cmdindex] = -1;
			stdout_redir[cmdindex] = -1;
		}
		argindex++;
	}

#if FALSE
	printf("parseargs() successful\n");
	for(int i=0; i<cmd_count; i++) {
		printf("argv[%d] = \"%s\"\t",cmd[i],argv[cmd[i]]);
		for(int j=0; argv[cmd[i]+j]; j++) {
			printf("\targv[%d] = \"%s\"",cmd[i]+j,argv[cmd[i]+j]);
		}
		printf("\n");
	}
#endif

	return cmdindex + 1;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
	static char array[MAXLINE]; /* holds local copy of command line */
	char *buf = array;          /* ptr that traverses command line */
	char *delim;                /* points to first space delimiter */
	int argc;                   /* number of args */
	int bg;                     /* background job? */

	strcpy(buf, cmdline);
	buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
	while (*buf && (*buf == ' ')) /* ignore leading spaces */
		buf++;

	/* Build the argv list */
	argc = 0;
	if (*buf == '\'') {
		buf++;
		delim = strchr(buf, '\'');
	}
	else {
		delim = strchr(buf, ' ');
	}

	while (delim) {
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* ignore spaces */
			buf++;

		if (*buf == '\'') {
			buf++;
			delim = strchr(buf, '\'');
		}
		else {
			delim = strchr(buf, ' ');
		}
	}
	argv[argc] = NULL;

	if (argc == 0)  /* ignore blank line */
		return 1;

#if FALSE
	printf("parseline() successful\n");
	for(int i=0; argv[i]; i++) {
		printf("argv[%d] = \"%s\"\n",i,argv[i]);
	}
#endif

	/* should the job run in the background? */
	if ((bg = (*argv[argc-1] == '&')) != 0) {
		argv[--argc] = NULL;
	}
	return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	if (!argv[0])
		return 0;
	if (strcmp(argv[0],"quit")==0)
		exit(0);
	return 0;     /* not a builtin command */
}

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
	printf("Usage: shell [-hvp]\n");
	printf("   -h   print this message\n");
	printf("   -v   print additional diagnostic information\n");
	printf("   -p   do not emit a command prompt\n");
	exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
	fprintf(stdout, "%s: %s\n", msg, strerror(errno));
	exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
	fprintf(stdout, "%s\n", msg);
	exit(1);
}

