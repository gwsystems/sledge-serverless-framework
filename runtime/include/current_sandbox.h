#ifndef SFRT_CURRENT_SANDBOX_H
#define SFRT_CURRENT_SANDBOX_H

#include "sandbox.h"
#include "types.h"

void                 current_sandbox_close_file_descriptor(int io_handle_index);
struct sandbox *     current_sandbox_get(void);
char *               current_sandbox_get_arguments(void);
int                  current_sandbox_get_file_descriptor(int io_handle_index);
int                  current_sandbox_get_http_request_body(char **body);
union uv_any_handle *current_sandbox_get_libuv_handle(int io_handle_index);
int                  current_sandbox_initialize_io_handle(void);
int                  current_sandbox_initialize_io_handle_and_set_file_descriptor(int file_descriptor);
int                  current_sandbox_parse_http_request(size_t length);
void                 current_sandbox_set(struct sandbox *sandbox);
int                  current_sandbox_set_file_descriptor(int io_handle_index, int file_descriptor);
int                  current_sandbox_set_http_response_header(char *header, int length);
int                  current_sandbox_set_http_response_body(char *body, int length);
int                  current_sandbox_set_http_response_status(char *status, int length);
int                  current_sandbox_vectorize_http_response(void);

#endif /* SFRT_CURRENT_SANDBOX_H */
