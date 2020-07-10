#pragma once

#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include "types.h"

extern int      runtime_epoll_file_descriptor;
extern uint32_t runtime_total_worker_processors;

void         alloc_linear_memory(void);
void         expand_memory(void);
INLINE char *get_function_from_table(uint32_t idx, uint32_t type_id);
INLINE char *get_memory_ptr_for_runtime(uint32_t offset, uint32_t bounds_check);
void         runtime_initialize(void);
void         listener_thread_initialize(void);
void         stub_init(i32 offset);

unsigned long long __getcycles(void);
