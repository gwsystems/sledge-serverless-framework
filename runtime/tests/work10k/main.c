#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUF 10240

//__attribute__((optnone)) int
int
main(void)
{
	char *d = malloc(MAX_BUF + 1);
	int   r = read(0, d, MAX_BUF);

	//	unsigned long long st = rdtsc(), en = 0;
	//	wrk();
	//	en = rdtsc();

	//	if (r <= 0) printf("%llu\n", en > st ? (en - st)/CPU_CYCS : -1);
	if (r < 0)
		printf("E\n");
	else if (r <= 1)
		printf("D\n");
	else
		write(1, d, r);

	return 0;
}
