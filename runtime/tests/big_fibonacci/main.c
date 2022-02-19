#include <stdio.h>
#include <stdlib.h>
// #include "get_time.h"
volatile unsigned long g_v = 0;
unsigned long int
/*fib(unsigned long int n)
{
	if (n <= 1) return n;
	return fib(n + g_v - 1) + fib(n + g_v - 2);
}*/


fib(unsigned long int n)
{
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
}


int
main(int argc, char **argv)
{
	char * array = NULL;
	unsigned long n = 0, r,r2,r3,r4,r5,r6,r7,r8,r9,r10;
	scanf("%lu", &n);
	//	unsigned long long st = get_time(), en;
	r = fib(n);
	r2 = fib(n+1);
	r3 = fib(n+2);
	r4 = fib(n+3);
	r5 = fib(n+4);
	r6 = fib(n+5);
	r7 = fib(n+6);
	r8 = fib(n+7);
	r9 = fib(n+8);
	r10 = fib(n+9);
        /*switch(n) {
                case 0: {
			array = malloc(4 * 1024);
			memset(array, 'a', 4 * 1024 - 1);
			array[4 * 1024 - 1] = 0;
			printf("%s\n", array);
			break;
		}
                case 1: {
			array = malloc(100 * 1024);
                        memset(array, 'b', 100 * 1024 - 1);
                        array[100 * 1024 - 1] = 0;
                        printf("%s\n", array);
			break;
		}
                case 2: {
			array = malloc(200 * 1024);
                        memset(array, 'g', 200 * 1024 - 1);
                        array[200 * 1024 - 1] = 0;
                        printf("%s\n", array);

			break;
		}
		case 4: {
                        array = malloc(400 * 1024);
                        memset(array, 'c', 400 * 1024 - 1);
                        array[400 * 1024 - 1] = 0;
                        printf("%s\n", array);

                        break;
                }

		case 6: {
                        array = malloc(600 * 1024);
                        memset(array, 'd', 600 * 1024 - 1);
                        array[600 * 1024 - 1] = 0;
                        printf("%s\n", array);

                        break;
                }

		case 8: {
                        array = malloc(800 * 1024);
                        memset(array, 'e', 800 * 1024 - 1);
                        array[800 * 1024 - 1] = 0;
                        printf("%s\n", array);

                        break;
                }

		case 10:{
                        array = malloc(1000 * 1024);
                        memset(array, 'f', 1000 * 1024 - 1);
                        array[1000 * 1024 - 1] = 0;
                        printf("%s\n", array);

                        break;
                }
			
		default: printf("error input of n\n");
        }*/
	//	en = get_time();
	printf("%lu \n", n+10);
	printf("%lu \n", r10);
	printf("%lu \n", r9);
	printf("%lu \n", r8);
	printf("%lu \n", r7);
	printf("%lu \n", r6);
	printf("%lu \n", r5);
	printf("%lu \n", r4);
	printf("%lu \n", r3);
	printf("%lu \n", r2);
	printf("%lu \n", r);
	//	print_time(st, en);
	return 0;
}
/*
int
main(int argc, char **argv)
{
	unsigned long n = 0, r = 0;
	scanf("%lu", &n);
        switch(n) {
                case 0: {
			char array[4 * 1024] = {0};
			memset(array, 'a', 4 * 1024);
			array[4 * 1024 - 1] = 0;
			break;
		}
                case 1: {
			char array[100 * 1024] = {'b'};
			memset(array, 'b', 100 * 1024);
                        array[100 * 1024 - 1] = 0;
			break;
		}
                case 2: {
			char array[200 * 1024] = {'c'};
			memset(array, 'c', 200 * 1024);
                        array[200 * 1024 - 1] = 0;
			break;
		}
		case 4: {
			char array[400 * 1024] = {'d'};
			memset(array, 'd', 400 * 1024);
                        array[400 * 1024 - 1] = 0;

                        break;
                }

		case 6: {
			char array[600 * 1024] = {'e'};
			memset(array, 'e', 600 * 1024);
                        array[600 * 1024 - 1] = 0;

                        break;
                }

		case 8: {
			char array[800 * 1024] = {'f'};
			memset(array, 'f', 800 * 1024);
                        array[800 * 1024 - 1] = 0;

                        break;
                }

		case 10:{
			char array[1000 * 1024] = {'g'};
			memset(array, 'g', 1000 * 1024);
                        array[1000 * 1024 - 1] = 0;

                        break;
                }
			
		default: printf("error input of n\n");
        }
	//	unsigned long long st = get_time(), en;
	printf("%lu \n", r);
	//	print_time(st, en);
	return 0;
}
*/
