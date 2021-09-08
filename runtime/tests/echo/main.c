#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1

// #include "get_time.h"
unsigned long int
fib(unsigned long int n)
{
	if (n <= 1) return n;
	return fib(n - 1) + fib(n - 2);
}

int
main(int argc, char **argv)
{
	extern int errno;
	char       buffer[BUF_SIZE];
	int        rc = 0;

	do {
		rc = read(STDIN_FILENO, &buffer, BUF_SIZE);
		printf("RC: %d\n", rc);
		// if (rc > 0) write(STDOUT_FILENO, &buffer, rc);
		// if (rc < 0) printf("Err: %s\n", strerror(errno));
	} while (rc != 0);

	return 0;
}
