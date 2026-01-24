#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

int main(int argc, char *argv[]) {
	int pid;

	FILE* out = fopen("fork-output.txt","w");
	fprintf(out,"BEFORE FORK(%d)\n",fileno(out));
	fflush(out);

	int pipefd[2];
	if (pipe(pipefd) < 0) {
		fprintf(stderr, "Could not pipe()\n");
		exit(1);
	}

	printf("Starting program; process has pid %d\n", getpid());
	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	printf("Section A;  pid %d\n", getpid());

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		printf("Section B\n");
		fprintf(out,"SECTION B(%d)\n",fileno(out));
		fflush(out);

		close(pipefd[0]);
		sleep(1);
		write(pipefd[1],"hello from Section B\n",21);
		sleep(1);
		close(pipefd[1]);

		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
		sleep(3);

		printf("Running exec of \"%s\"\n", argv[1]);
		dup2(fileno(out),STDOUT_FILENO);
		close(fileno(out));
		if (argc <= 1) {
			printf("No program to exec.  Exiting...\n");
			exit(0);
		}
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);

		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */
		printf("Section C\n");
		fprintf(out,"SECTION C(%d)\n",fileno(out));
		fclose(out);

		close(pipefd[1]);
		char msg[22];

		int msglen = read(pipefd[0],msg,21);
		printf("parent read %d bytes from child:\n --> ",msglen);
		msg[msglen] = '\0';
		printf(msg);

		msglen = read(pipefd[0],msg,21);
		printf("parent read %d bytes from child\n",msglen);

		close(pipefd[0]);
		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	printf("Section D\n");

	/* END SECTION D */
}

