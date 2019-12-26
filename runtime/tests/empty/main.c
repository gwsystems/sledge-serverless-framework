#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUF (1024*1024) //1m

int
main(int argc, char **argv)
{
	char *d = malloc(MAX_BUF + 1);
	int r = read(0, d, MAX_BUF);
	if (r <= 0) printf("%s\n", r == 0 ? "empty" : "error");
	else        write(1, d, MAX_BUF);

	return 0;
}
