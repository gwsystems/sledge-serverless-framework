#include <stdio.h>
#include <stdlib.h>

// #include "get_time.h"
unsigned long int
fib(unsigned long int n)
{
	if (n <= 1) return n;
	return fib(n - 1) + fib(n - 2);
}
/*
int
main(int argc, char **argv)
{
	unsigned long r = 0;
        //scanf("%s", recv_buf);
	r = fib(30);
	printf("%lu\n", r);
	return 0;
}*/
int
main(int argc, char **argv)
{
	//char * recv_buf = malloc(1024 * 1024);
	char recv_buf[1024 * 1024] = {0};
	//memset(recv_buf, 0, 1024 * 1024);
        unsigned long r = 0;
        //scanf("%s", recv_buf);
	r = read(0, recv_buf, 1024 * 1024);
	//size_t rd = read(0, recv_buf, 1000*1024);
        //if (rd <= 0) return -1;

        //      unsigned long long st = get_time(), en;
        //r = fib(30);
        //      en = get_time();
        printf("%lu\n", r);

	//	print_time(st, en);
	return 0;
}
