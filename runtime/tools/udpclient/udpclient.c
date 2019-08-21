#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define MSG_MAX 1024

static char *
remove_spaces(char *str)
{
        int i = 0;
        while (isspace(*str)) str++;
        i = strlen(str);
        while (isspace(str[i-1])) str[i-1]='\0';

        return str;
}

int main(int argc, char *argv[])
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

	while (1) {
		fseek(f, 0, SEEK_SET);

		char line[MSG_MAX] = { 0 };
		while (fgets(line, MSG_MAX, f) != NULL) {
			int fd = -1;
			struct sockaddr_in sa;
			char *msg = NULL, *tok, *src = line;
			char ip[32] = { 0 }, port[32] = { 0 };
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
				printf("Proceeding!\n");
			} else {
				printf("Skipping!\n");
				goto next;
			}

			sa.sin_family      = AF_INET;
			sa.sin_port        = htons(atoi(port));
			sa.sin_addr.s_addr = inet_addr(ip);
			if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
				perror("Establishing socket");
				return -1;
			}

			if (sendto(fd, msg, strlen(msg), 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
					errno != EINTR) {
				perror("sendto");
				return -1;
			}
			printf("Done!\n");
next:
			memset(line, 0, MSG_MAX);
			fflush(stdin);
			fflush(stdout);

			if (fd >= 0) close(fd);
		}
	}

	fclose(f);
	
	return 0;
}
