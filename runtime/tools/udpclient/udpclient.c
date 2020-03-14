#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define MSG_MAX 1024
#define STR_MAX 32

struct request {
	char ip[32];
	char port[32];
	char msg[MSG_MAX];
};

static char *
remove_spaces(char *str)
{
	int i = 0;
	while (isspace(*str)) str++;
	i = strlen(str);
	while (isspace(str[i - 1])) str[i - 1] = '\0';

	return str;
}

void *
send_fn(void *d)
{
	struct request *r = (struct request *)d;

	char               resp[STR_MAX] = { 0 };
	int                file_descriptor            = -1;
	struct sockaddr_in sa;

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(atoi(r->port));
	sa.sin_addr.s_addr = inet_addr(r->ip);
	if ((file_descriptor = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return NULL;
	}

	if (sendto(file_descriptor, r->msg, strlen(r->msg), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0 && errno != EINTR) {
		perror("sendto");
		return NULL;
	}

	int sa_len = sizeof(sa);
	if (recvfrom(file_descriptor, resp, STR_MAX, 0, (struct sockaddr *)&sa, &sa_len) < 0) { perror("recvfrom"); }
	printf("Done[%s]!\n", resp);
	close(file_descriptor);
	free(r);

	return NULL;
}

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <sandbox_file>\n", argv[0]);
		return -1;
	}

	FILE *f = fopen(argv[1], "r");
	if (!f) {
		perror("fopen");
		return -1;
	}

	while (true) {
		fseek(f, 0, SEEK_SET);

		char line[MSG_MAX] = { 0 };
		while (fgets(line, MSG_MAX, f) != NULL) {
			char *msg = NULL, *tok, *src = line;
			char  ip[STR_MAX] = { 0 }, port[STR_MAX] = { 0 };
			src = remove_spaces(src);

			if (src[0] == ';') goto next;
			tok = strtok_r(src, ":", &src);
			strcpy(ip, tok);
			tok = strtok_r(src, "$", &src);
			strcpy(port, tok);
			msg = src;

			int i = 0;
			printf("\nRequest [%s] to [%s:%d]\n (1:Proceed 2:Skip ANY:Exit) ", msg, ip, atoi(port));
			scanf("%d", &i);
			if (i <= 0 || i > 2) {
				printf("Exiting!\n");
				exit(0);
			} else if (i == 1) {
				pthread_t t;
				printf("Proceeding!\n");
				struct request *r = (struct request *)malloc(sizeof(struct request));
				strncpy(r->ip, ip, STR_MAX);
				strncpy(r->port, port, STR_MAX);
				strncpy(r->msg, msg, MSG_MAX);
				pthread_create(&t, NULL, send_fn, r);
			} else {
				printf("Skipping!\n");
				goto next;
			}

		next:
			memset(line, 0, MSG_MAX);
			fflush(stdin);
			fflush(stdout);
		}
	}

	fclose(f);

	return 0;
}
