#include <stdlib.h>

int
main(int argc, char **argv)
{
	printf("Should not print after this\n");
	exit(EXIT_SUCCESS);
	printf("FAIL\n");
}
