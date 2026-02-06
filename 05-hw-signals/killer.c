#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>

void sigint_handler(int signum) {
	// send SIGKILL to all processes in group, so this process and children
	// will terminate.  Use SIGKILL because SIGTERM and SIGINT (among
	// others) are overridden in the child.
	kill(-getpgid(0), SIGKILL);
}

int main(int argc, char *argv[]) {
	char *scenario = argv[1];
	int pid = atoi(argv[2]);

	struct sigaction sigact;

	// Explicitly set flags
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = sigint_handler;
	// Override SIGINT, so that interrupting this process sends SIGKILL to
	// this one and, more importantly, to the child.
	sigaction(SIGINT, &sigact, NULL);

	//int h[10] = {-1,SIGHUP,SIGQUIT,SIGTERM,30,10,16,31,12,SIGCHLD};
	/* Signal-to-handler dictionary:
	 * [handler#] = [SIG#]
	 * 1 = SIGHUP | SIGINT
	 * 2 = SIGQUIT
	 * 3 = SIGTERM
	 * 4 = 30
	 * 5 = 10
	 * 6 = 16
	 * 7 = 31
	 * 8 = 12
	 * 9 = SIGCHLD
	 *
	 * cool vim macro I used:
	 * while writing this I didn't want to keep track of the mapping between
	 * signal codes and handlers, so I defined int h[10] above
	 * and used h[#] values instead of the codes in my kill() statements.
	 * Once I was finished I wanted to replace them all at once, so I wrote
	 * this vim macro:
	 *     :let @j=line(".")29gg0:normal jf,€ý5l"kye:%s/h\[j\]/kj
	 * Then I positioned myself at line 1 and ran it 9 times.
	 * The macro copies down my current line as the handler index,
	 * goes to the declaration of h[] and yanks the appropriate signal code,
	 * and does a document-wide search-and-replace for that code.
	 * Finally it ends one line lower, so I can run it immediately again.
	 * It was a fun puzzle to figure out how to do this, I thought Dr. Deccio
	 * or any TA who uses vim might get a kick out of that.
	 */

	switch (scenario[0]) {
	case '0':
		kill(pid, SIGHUP);
		break;
	case '1':
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '2':
		kill(pid, SIGHUP);
		sleep(5);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '3':
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, SIGHUP);
		sleep(8);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '4':
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, SIGINT);
		sleep(8);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '5':
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '6':
		kill(pid, SIGHUP);
		sleep(5);
		kill(pid, 10);
		sleep(1);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '7':
		kill(pid, SIGHUP);
		sleep(5);
		kill(pid, 10);
		sleep(1);
		kill(pid, 16);
		sleep(1);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '8':
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, 31);
		sleep(5);
		kill(pid, 10);
		sleep(1);
		kill(pid, 30);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	case '9': 
		kill(pid, 31);
		sleep(1);
		kill(pid, SIGQUIT);
		sleep(5);
		kill(pid, 31);
		sleep(5);
		kill(pid, 12);
		sleep(1);
		kill(pid, SIGTERM);
		break;

	}
	waitpid(pid, NULL, 0);
}
