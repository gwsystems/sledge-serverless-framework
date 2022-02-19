#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
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

//int
//main(int argc, char **argv)
//{
//        char * array = NULL;
//	unsigned long n = 0, r;
//        scanf("%lu", &n);
        //      unsigned long long st = get_time(), en;
//        r = fib(n);
        //      en = get_time();
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
        }
	free(array);*/
	//printf("%lu\n", n);
//        printf("%lu\n", n+1);
//	printf("%lu\n", r);

	//return 0;
//}

main(int argc, char **argv)
{
	unsigned long n = 0, r;
        scanf("%lu", &n);
	FILE *f = stdout;
        //      unsigned long long st = get_time(), en;
        //r = fib(29);
        //      en = get_time();
        switch(n) {
                case 0: {
			char array[4 * 1024] = {0};
			memset(array, 'a', 4 * 1024);
			array[4 * 1024 - 1] = 0;
			//printf("%s\n", array);
			fwrite(array, 1, 4 * 1024 - 1, f);
			break;
		}
                case 1: {
			char array[100 * 1024] = {'b'};
			memset(array, 'b', 100 * 1024);
                        array[100 * 1024 - 1] = 0;
                        //printf("%s\n", array);
			fwrite(array, 1, 100 * 1024 - 1, f);
			break;
		}
                case 2: {
			char array[200 * 1024] = {'c'};
			memset(array, 'c', 200 * 1024);
                        array[200 * 1024 - 1] = 0;
			fwrite(array, 1, 200 * 1024 - 1, f);
                        //printf("%s\n", array);

			break;
		}
		case 4: {
			char array[400 * 1024] = {'d'};
			memset(array, 'd', 400 * 1024);
                        array[400 * 1024 - 1] = 0;
			fwrite(array, 1, 400 * 1024 - 1, f);
                        //printf("%s\n", array);

                        break;
                }

		case 6: {
			char array[600 * 1024] = {'e'};
			memset(array, 'e', 600 * 1024);
                        array[600 * 1024 - 1] = 0;
			fwrite(array, 1, 600 * 1024 - 1, f);
                        //printf("%s\n", array);

                        break;
                }

		case 8: {
			char array[800 * 1024] = {'f'};
			memset(array, 'f', 800 * 1024);
                        array[800 * 1024 - 1] = 0;
			fwrite(array, 1, 800 * 1024 - 1, f);
                        //printf("%s\n", array);

                        break;
                }

		case 10:{
			char array[1000 * 1024] = {'g'};
			memset(array, 'g', 1000 * 1024);
                        array[1000 * 1024 - 1] = 0;
			fwrite(array, 1, 1000 * 1024 - 1, f);
                        //printf("%s\n", array);

                        break;
                }
			
		default: printf("error input of n\n");
        }
	fclose(f);
	//printf("%lu\n", n);
        //printf("%lu\n", r);

	return 0;
}
