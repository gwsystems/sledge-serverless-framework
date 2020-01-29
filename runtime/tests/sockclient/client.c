/* code from http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

void
error(char *msg)
{
	perror(msg);
	exit(0);
}

int
main(int argc, char *argv[])
{
	int sockfd, portno, n;

	struct sockaddr_in serv_addr;
	struct hostent *   server;

	char buffer[256] = "The quick brown fox jumps over the lazy dog";
	if (argc < 3) {
		fprintf(stderr, "usage %s hostname port\n", argv[0]);
		exit(0);
	}
	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	server = gethostbyname(argv[1]);
	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR connecting");
	printf("Sending message %s\n", buffer);
	n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR writing to socket");
	bzero(buffer, 256);
	n = recv(sockfd, buffer, 255, 0);
	if (n < 0) error("ERROR reading from socket");
	printf("%s\n", buffer);
	close(sockfd);
	return 0;
}
