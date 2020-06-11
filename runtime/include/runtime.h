#ifndef SFRT_RUNTIME_H
#define SFRT_RUNTIME_H

#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include "types.h"

extern int runtime_epoll_file_descriptor;

void         alloc_linear_memory(void);
void         expand_memory(void);
INLINE char *get_function_from_table(uint32_t idx, uint32_t type_id);
INLINE char *get_memory_ptr_for_runtime(uint32_t offset, uint32_t bounds_check);
void         runtime_initialize(void);
void         listener_thread_initialize(void);
void         stub_init(int32_t offset);

unsigned long long __getcycles(void);

#endif /* SFRT_RUNTIME_H */
