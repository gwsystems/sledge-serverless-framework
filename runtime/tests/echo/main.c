#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1000

int
main(int argc, char **argv)
{
	extern int errno;
	char       buffer[BUF_SIZE];
	int        rc = 0;

	do {
		rc = read(STDIN_FILENO, &buffer, BUF_SIZE);
		if (rc > 0) write(STDOUT_FILENO, &buffer, rc);
	} while (rc > 0);

	return rc;
}
