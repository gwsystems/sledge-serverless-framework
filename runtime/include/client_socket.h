#pragma once

#include <sys/socket.h>

void client_socket_close(int client_socket, struct sockaddr *client_address);

int client_socket_send(int client_socket, int status_code);
