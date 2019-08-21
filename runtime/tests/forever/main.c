#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ITERS 50000000

int
main(int argc, char **argv) __attribute__ ((optnone))
{
	printf("%s enter\n", argv[0]);
	int n = 0, e = 1;
	if (argc == 2) {
		n = atoi(argv[1]);
		if (n > 0) e = 0;
	}

	while (e || n > 0) {
		int i = ITERS;
		n--;
		while (i-- > 0) {
			int j = ITERS;
			while (j-- > 0) __asm__ __volatile__("nop": : :"memory");
		}
		printf("%s\n", argv[0]);
	}
	printf("%s done\n", argv[0]);
}
