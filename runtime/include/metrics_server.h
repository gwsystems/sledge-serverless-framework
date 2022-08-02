#pragma once

#include "tcp_server.h"

extern struct tcp_server metrics_server;

void metrics_server_init();
void metrics_server_thread_spawn(int client_socket);
int  metrics_server_close();
