#ifndef SFRT_SANDBOX_REQUEST_QUEUE_H
#define SFRT_SANDBOX_REQUEST_QUEUE_H

#include <sandbox_request.h>

void               sandbox_request_queue_initialize(void);
int                sandbox_request_queue_add(sandbox_request_t *);
sandbox_request_t *sandbox_request_queue_remove(void);

#endif /* SFRT_SANDBOX_REQUEST_QUEUE_H */