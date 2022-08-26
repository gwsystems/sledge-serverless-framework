#pragma once

#include "epoll_tag.h"
#include "tcp_server.h"

struct metrics_server {
	enum epoll_tag    tag;
	struct tcp_server tcp;
	pthread_attr_t    thread_settings;
};


extern struct metrics_server metrics_server;

void metrics_server_init();
void metrics_server_thread_spawn(int client_socket);
int  metrics_server_close();
