#pragma once

#include "tcp_server.h"

extern struct tcp_server metrics_server;

void metrics_server_init();
int  metrics_server_listen();
int  metrics_server_close();
void metrics_server_handler(int client_socket);
