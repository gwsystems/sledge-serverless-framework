#pragma once

#include "sandbox.h"

void                 current_sandbox_close_file_descriptor(int io_handle_index);
struct sandbox *     current_sandbox_get(void);
int                  current_sandbox_get_file_descriptor(int io_handle_index);
int                  current_sandbox_initialize_io_handle(void);
void                 current_sandbox_set(struct sandbox *sandbox);
int                  current_sandbox_set_file_descriptor(int io_handle_index, int file_descriptor);
