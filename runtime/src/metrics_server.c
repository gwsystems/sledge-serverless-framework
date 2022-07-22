#include <stdlib.h>
#include <unistd.h>

#include "tcp_server.h"
#include "http_total.h"

struct tcp_server metrics_server;

void
metrics_server_init()
{
	tcp_server_init(&metrics_server, 1776);
}

int
metrics_server_listen()
{
	return tcp_server_listen(&metrics_server);
}

int
metrics_server_close()
{
	return tcp_server_close(&metrics_server);
}

void
metrics_server_handler(int client_socket)
{
	int rc = 0;

	char  *ostream_base = NULL;
	size_t ostream_size = 0;
	FILE  *ostream      = open_memstream(&ostream_base, &ostream_size);
	assert(ostream != NULL);

	uint32_t total_reqs = atomic_load(&http_total_requests);
	uint32_t total_5XX  = atomic_load(&http_total_5XX);

	fprintf(ostream, "HTTP/1.1 200 OK\r\n\r\n");

	fprintf(ostream, "# TYPE total_requests counter\n");
	fprintf(ostream, "total_requests: %d\n", total_reqs);

	fprintf(ostream, "# TYPE total_rejections counter\n");
	fprintf(ostream, "total_rejections: %d\n", total_5XX);
	fflush(ostream);

	rewind(ostream);

	char   buf[256] = { 0 };
	size_t nread    = 0;
	do {
		nread      = fread(buf, 1, 255, ostream);
		buf[nread] = '\0';
		/* TODO: Deal with blocking here! */
		write(client_socket, buf, nread);
	} while (nread > 0);

	rc = fclose(ostream);
	assert(rc == 0);

	free(ostream_base);
	ostream_size = 0;

	close(client_socket);
}
