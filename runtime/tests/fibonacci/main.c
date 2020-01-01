#include <stdio.h> 
#include "get_time.h"
unsigned long int
fib(unsigned long int n) 
{ 
	if (n <= 1) 
		return n; 
	return fib(n-1) + fib(n-2); 
} 

int
main(int argc, char **argv) 
{ 
	unsigned long n = 0, r;
	scanf("%lu", &n);
	unsigned long long st = get_time(), en;
	r = fib(n);
	en = get_time();
	printf("%lu\n", r);

	print_time(st, en);
	return 0; 
} 
