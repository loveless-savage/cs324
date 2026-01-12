#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>

#define BUFSIZE 30

void memprint(char *, char *, int);

void part1();
void part3(char *);
void part4();

int main(int argc, char *argv[]) {
	part1();

	/* Part 2 (below this comment) */
	printf("\n===== Part 2 =====\n");
	printf("-- Questions 8 and 9\n");
	printf("argc = %x\n",argc);
	for(int i=0; i<argc; i++) {
		printf("%d: %s\n",i,argv[i]);
	}

	printf("-- Questions 10 and 11\n");
	if(argc!=2){
		fprintf(stderr,"Exactly one command-line option is required: filename\n");
		exit(6);
	}

	part3(argv[1]);
	part4();
}

void memprint(char *s, char *fmt, int len) {
	// iterate through each byte/character of s, and print each out with
	// the format specified with fmt
	int i;
	char fmt_with_space[8];

	sprintf(fmt_with_space, "%s ", fmt);
	for (i = 0; i < len; i++) {
		printf(fmt_with_space, s[i]);
	}
	printf("\n");
}

void part1() {
	printf("\n===== Part 1 =====\n");

	// Note: STDOUT_FILENO is defined in /usr/include/unistd.h:
	//
	// #define	STDOUT_FILENO	1

	char s1a[] = { 104, 101, 108, 108, 111, 10 };
	write(STDOUT_FILENO, s1a, 6);

	char s1b[] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x0a };
	write(STDOUT_FILENO, s1b, 6);

	char s1c[] = { 'h', 'e', 'l', 'l', 'o', '\n' };
	write(STDOUT_FILENO, s1c, 6);

	char s2[] = { 0xe5, 0x8f, 0xb0, 0xe7, 0x81, 0xa3, 0x0a };
	write(STDOUT_FILENO, s2, 7);

	char s3[] = { 0xf0, 0x9f, 0x98, 0x82, 0x0a };
	write(STDOUT_FILENO, s3, 5);

	int s1a_len = sizeof(s1a);

	printf("-- Questions 1 through 4\n");
	memprint(s1a,"%02x",s1a_len);
	memprint(s1a,"%d",s1a_len);
	memprint(s1a,"%c",s1a_len);

	printf("-- Questions 5 through 7\n");
	printf("%02x\n",'B');
	printf("%d\n",'$');
	printf("%c\n",7);

}

void part3(char *filename) {
	printf("\n===== Part 3 =====\n");

	printf("-- Question 12\n");
	printf("stdin  --> %d\n",fileno(stdin));
	printf("stdout --> %d\n",fileno(stdout));
	printf("stderr --> %d\n",fileno(stderr));

	printf("-- Question 13\n");
	char *buf = malloc(BUFSIZE*sizeof(char));
	memset(buf,'z',BUFSIZE-1);
	buf[BUFSIZE-1] = '\0';
	memprint(buf,"%02x",BUFSIZE);

	printf("-- Question 14\n");
	printf("%s",buf);
	printf("\n");
	write(STDOUT_FILENO,buf,BUFSIZE);
	printf("\nTODO\n");

	fprintf(stderr, "-- Questions 15 through 18\n");
	fprintf(stderr,"%s\n",buf);
	write(STDERR_FILENO,buf,BUFSIZE);
	fprintf(stderr,"\n");

	printf("-- Question 19\n");
	int fd1, fd2;
	fd1 = open(filename,O_RDONLY);
	printf("fd1 = %d\n",fd1);
	fd2 = fd1;
	printf("fd2 = %d\n",fd2);

	printf("-- Questions 20 and 21\n");
	size_t numread = read(fd1,buf,4);
	printf("(fd1) numread = %d\n",numread);
	size_t totread = numread;
	printf("totread = %d\n",totread);
	memprint(buf,"%02x",BUFSIZE);

	printf("-- Questions 22 through 25\n");
	numread = read(fd2,buf+totread,4);
	printf("(fd2) numread = %d\n",numread);
	totread += numread;
	printf("totread = %d\n",totread);
	memprint(buf,"%02x",BUFSIZE);

	printf("-- Questions 26 through 31\n");
	numread = read(fd1, buf+totread, BUFSIZE-totread);
	printf("(fd1) numread = %d\n",numread);
	totread += numread;
	printf("totread = %d\n",totread);
	memprint(buf,"%02x",BUFSIZE);

	printf("-- Questions 32 and 33\n");
	numread = read(fd1, buf+totread, BUFSIZE-totread);
	printf("(fd1) numread = %d\n",numread);
	totread += numread;
	printf("totread = %d\n",totread);
	memprint(buf,"%02x",BUFSIZE);

	printf("-- Question 34\n");
	printf("%s\n",buf);

	printf("-- Question 35\n");
	buf[totread] = '\0';
	printf("%s\n",buf);

	printf("-- Question 36\n");
	int ret = close(fd1);
	printf("close(fd1) --> %d\n",ret);

	printf("-- Question 37\n");
	ret = close(fd2);
	printf("close(fd2) --> %d\n",ret);

	printf("-- Question 38\n");
	fprintf(stdout,"abc");
	fprintf(stderr,"def");
	fprintf(stdout,"ghi\n");

	printf("-- Questions 39 and 40\n");
	write(STDOUT_FILENO,"abc",3);
	write(STDERR_FILENO,"def",3);
	write(STDOUT_FILENO,"ghi",3);
	printf("\n");

	printf("-- Question 41\n");
	fprintf(stdout,"abc");
	fflush(stdout);
	fprintf(stderr,"def");
	fprintf(stdout,"ghi\n");

	free(buf);
}

void part4() {
	printf("\n===== Part 4 =====\n");

	printf("-- Questions 42 and 43\n");
	char *s1 = getenv("CS324_VAR");
	if (s1 == NULL) {
		printf("CS324_VAR not found\n");
	} else {
		printf("CS324_VAR is %s\n",s1);
	}
}
