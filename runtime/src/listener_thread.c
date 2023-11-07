#include <stdint.h>
#include <unistd.h>

#include "erpc_handler.h"
#include "erpc_c_interface.h"
#include "arch/getcycles.h"
#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "metrics_server.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "tcp_session.h"
#include "tenant.h"
#include "tenant_functions.h"
#include "http_session_perf_log.h"
#include "sandbox_set_as_runnable.h"
#include "request_typed_queue.h"
#include "request_typed_deque.h"
#include "local_preempted_fifo_queue.h"

#define INTERRUPT_INTERVAL 50
#define BASE_SERVICE_TIME 10
#define LOSS_PERCENTAGE 0.11

thread_local int next_loop_start_index = -1;
thread_local uint64_t total_requests = 0;
uint64_t base_simulated_service_time = 0;
uint64_t shinjuku_interrupt_interval = 0; 
thread_local uint32_t global_queue_length = 0;
thread_local uint32_t max_queue_length = 0;

uint32_t worker_old_sandbox[1024] = {0};
uint32_t worker_new_sandbox[1024] = {0};
struct perf_window * worker_perf_windows[1024];
struct priority_queue * worker_queues[1024];
struct binary_tree * worker_binary_trees[1024];
struct request_fifo_queue * worker_circular_queue[1024];
struct ps_list_head * worker_lists[1024];
struct request_fifo_queue * worker_preempted_queue[1024];

extern FILE *sandbox_perf_log;
extern bool runtime_exponential_service_time_simulation_enabled;
extern _Atomic uint64_t worker_queuing_cost[1024]; 
//struct sandbox *urgent_request[1024] = { NULL };
extern uint32_t runtime_worker_threads_count;
extern thread_local bool pthread_stop;
extern _Atomic uint64_t request_index;
extern uint32_t runtime_worker_group_size;
extern struct sandbox* current_sandboxes[1024];

thread_local uint32_t dispatcher_try_interrupts = 0;
thread_local uint32_t worker_start_id;
thread_local uint32_t worker_end_id;
thread_local uint32_t worker_list[MAX_WORKERS]; // record the worker's true id(0 - N workers - 1). worker_list[0] - worker_list[2]
_Atomic uint32_t free_workers[MAX_DISPATCHER] = {0}; // the index is the dispatercher id, free_workers[dispatcher_id] 
					 // is decimal value of the bitmap of avaliable workers.
					 // For example, if there are 3 workers available, the bitmap 
					 // will be 111, then the free_workers[dispatcher_id] is 7


thread_local struct request_typed_queue *request_type_queue[MAX_REQUEST_TYPE];
thread_local struct request_typed_deque *request_type_deque[MAX_REQUEST_TYPE];

thread_local uint32_t n_rtypes = 0;

time_t t_start;
extern bool first_request_comming;
extern pthread_t *runtime_listener_threads;
static void listener_thread_unregister_http_session(struct http_session *http);
static void panic_on_epoll_error(struct epoll_event *evt);

static void on_client_socket_epoll_event(struct epoll_event *evt);
static void on_tenant_socket_epoll_event(struct epoll_event *evt);
static void on_client_request_arrival(int client_socket, const struct sockaddr *client_address, struct tenant *tenant);
static void on_client_request_receiving(struct http_session *session);
static void on_client_request_received(struct http_session *session);
static void on_client_response_header_sending(struct http_session *session);
static void on_client_response_body_sending(struct http_session *session);
static void on_client_response_sent(struct http_session *session);

/*
 * Descriptor of the epoll instance used to monitor the socket descriptors of registered
 * serverless modules. The listener cores listens for incoming client requests through this.
 */
int listener_thread_epoll_file_descriptor;

thread_local uint8_t dispatcher_thread_idx;
thread_local pthread_t listener_thread_id;
thread_local bool is_listener = false;


void typed_queue_init() {
        tenant_database_foreach(tenant_request_typed_queue_init, NULL, NULL);
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread_initialize(uint8_t thread_id)
{
	printf("Starting listener thread\n");
	
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(LISTENER_THREAD_START_CORE_ID + thread_id, &cs);

	/* Setup epoll */
	listener_thread_epoll_file_descriptor = epoll_create1(0);
	assert(listener_thread_epoll_file_descriptor >= 0);

	/* Pass the value we want the threads to use when indexing into global arrays of per-thread values */
        runtime_listener_threads_argument[thread_id] = thread_id;

	int ret = pthread_create(&runtime_listener_threads[thread_id], NULL, listener_thread_main, (void *)&runtime_listener_threads_argument[thread_id]);
	listener_thread_id = runtime_listener_threads[thread_id];
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread_id, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	printf("\tListener core thread: %lx\n", listener_thread_id);
}

/**
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 **/
void
listener_thread_register_http_session(struct http_session *http)
{
	assert(http != NULL);

	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register an http session before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)http;

	switch (http->state) {
	case HTTP_SESSION_RECEIVING_REQUEST:
		accept_evt.events = EPOLLIN;
		http->state       = HTTP_SESSION_RECEIVE_REQUEST_BLOCKED;
		break;
	case HTTP_SESSION_SENDING_RESPONSE_HEADER:
		accept_evt.events = EPOLLOUT;
		http->state       = HTTP_SESSION_SEND_RESPONSE_HEADER_BLOCKED;
		break;
	case HTTP_SESSION_SENDING_RESPONSE_BODY:
		accept_evt.events = EPOLLOUT;
		http->state       = HTTP_SESSION_SEND_RESPONSE_BODY_BLOCKED;
		break;
	default:
		panic("Invalid HTTP Session State: %d\n", http->state);
	}

	rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, http->socket, &accept_evt);
	if (rc != 0) { panic("Failed to add http session to listener thread epoll\n"); }
}

/**
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 **/
static void
listener_thread_unregister_http_session(struct http_session *http)
{
	assert(http != NULL);

	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to unregister an http session before listener thread initialization");
	}

	int rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_DEL, http->socket, NULL);
	if (rc != 0) { panic("Failed to remove http session from listener thread epoll\n"); }
}

/**
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 * Assumption: We never have to unregister a tenant
 **/
int
listener_thread_register_tenant(struct tenant *tenant)
{
	assert(tenant != NULL);
	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register a tenant before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)tenant;
	accept_evt.events   = EPOLLIN;
	rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, tenant->tcp_server.socket_descriptor,
	               &accept_evt);

	return rc;
}

int
listener_thread_register_metrics_server()
{
	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register metrics_server before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)&metrics_server;
	accept_evt.events   = EPOLLIN;
	rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, metrics_server.tcp.socket_descriptor,
	               &accept_evt);

	return rc;
}

static void
panic_on_epoll_error(struct epoll_event *evt)
{
	/* Check Event to determine if epoll returned an error */
	if ((evt->events & EPOLLERR) == EPOLLERR) {
		int       error  = 0;
		socklen_t errlen = sizeof(error);
		if (getsockopt(evt->data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
			panic("epoll_wait: %s\n", strerror(error));
		}
		debuglog("epoll_error: Most likely client disconnected. Closing session.");
	}
}

static void
on_client_request_arrival(int client_socket, const struct sockaddr *client_address, struct tenant *tenant)
{
	uint64_t request_arrival_timestamp = __getcycles();

	http_total_increment_request();

	/* Allocate HTTP Session */
	struct http_session *session = http_session_alloc(client_socket, (const struct sockaddr *)&client_address,
	                                                  tenant, request_arrival_timestamp);
	if (likely(session != NULL)) {
		on_client_request_receiving(session);
		return;
	} else {
		/* Failed to allocate memory */
		debuglog("Failed to allocate http session\n");
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 500);
		on_client_response_header_sending(session);
		return;
	}
}

static void
on_client_request_receiving(struct http_session *session)
{
	/* Read HTTP request */
	int rc = http_session_receive_request(session, (void_star_cb)listener_thread_register_http_session);
	if (likely(rc == 0)) {
		on_client_request_received(session);
		return;
	} else if (unlikely(rc == -EAGAIN)) {
		/* session blocked and registered to epoll so continue to next handle */
		return;
	} else if (rc < 0) {
		debuglog("Failed to receive or parse request\n");
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 400);
		on_client_response_header_sending(session);
		return;
	}

	assert(0);
}

static void
on_client_request_received(struct http_session *session)
{
	assert(session->state == HTTP_SESSION_RECEIVED_REQUEST);
	session->request_downloaded_timestamp = __getcycles();

	struct route *route = http_router_match_route(&session->tenant->router, session->http_request.full_url);
	if (route == NULL) {
		debuglog("Did not match any routes\n");
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 404);
		on_client_response_header_sending(session);
		return;
	}

	session->route = route;

	/*
	 * Perform admissions control.
	 * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
	 * TODO: Consider providing a Retry-After header
	 */
	uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
	if (work_admitted == 0) {
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 429);
		on_client_response_header_sending(session);
		return;
	}

	/* Allocate a Sandbox */
	session->state          = HTTP_SESSION_EXECUTING;
	struct sandbox *sandbox = sandbox_alloc(route->module, session, route, session->tenant, work_admitted, NULL, 0);
	if (unlikely(sandbox == NULL)) {
		debuglog("Failed to allocate sandbox\n");
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 500);
		on_client_response_header_sending(session);
		return;
	}

	/* If the global request scheduler is full, return a 429 to the client */
	if (unlikely(global_request_scheduler_add(sandbox) == NULL)) {
		debuglog("Failed to add sandbox to global queue\n");
		uint64_t ret[5] = {0};
		sandbox_free(sandbox, ret);
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 429);
		on_client_response_header_sending(session);
	}
}

static void
on_client_response_header_sending(struct http_session *session)
{
	int rc = http_session_send_response_header(session, (void_star_cb)listener_thread_register_http_session);
	if (likely(rc == 0)) {
		on_client_response_body_sending(session);
		return;
	} else if (unlikely(rc == -EAGAIN)) {
		/* session blocked and registered to epoll so continue to next handle */
		return;
	} else if (rc < 0) {
		http_session_close(session);
		http_session_free(session);
		return;
	}
}

static void
on_client_response_body_sending(struct http_session *session)
{
	/* Read HTTP request */
	int rc = http_session_send_response_body(session, (void_star_cb)listener_thread_register_http_session);
	if (likely(rc == 0)) {
		on_client_response_sent(session);
		return;
	} else if (unlikely(rc == -EAGAIN)) {
		/* session blocked and registered to epoll so continue to next handle */
		return;
	} else if (unlikely(rc < 0)) {
		http_session_close(session);
		http_session_free(session);
		return;
	}
}

static void
on_client_response_sent(struct http_session *session)
{
	assert(session->state = HTTP_SESSION_SENT_RESPONSE_BODY);

	/* Terminal State Logging for Http Session */
	session->response_sent_timestamp = __getcycles();
	http_session_perf_log_print_entry(session);

	http_session_close(session);
	http_session_free(session);
	return;
}

static void
on_tenant_socket_epoll_event(struct epoll_event *evt)
{
	assert((evt->events & EPOLLIN) == EPOLLIN);

	struct tenant *tenant = evt->data.ptr;
	assert(tenant);

	/* Accept Client Request as a nonblocking socket, saving address information */
	struct sockaddr_in client_address;
	socklen_t          address_length = sizeof(client_address);

	/* Accept as many clients requests as possible, returning when we would have blocked */
	while (true) {
		int client_socket = accept4(tenant->tcp_server.socket_descriptor, (struct sockaddr *)&client_address,
		                            &address_length, SOCK_NONBLOCK);
		if (unlikely(client_socket < 0)) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) return;

			panic("accept4: %s", strerror(errno));
		}

		on_client_request_arrival(client_socket, (const struct sockaddr *)&client_address, tenant);
	}
}

static void
on_metrics_server_epoll_event(struct epoll_event *evt)
{
	assert((evt->events & EPOLLIN) == EPOLLIN);

	/* Accept Client Request as a nonblocking socket, saving address information */
	struct sockaddr_in client_address;
	socklen_t          address_length = sizeof(client_address);

	/* Accept as many clients requests as possible, returning when we would have blocked */
	while (true) {
		/* We accept the client connection with blocking semantics because we spawn ephemeral worker threads */
		int client_socket = accept4(metrics_server.tcp.socket_descriptor, (struct sockaddr *)&client_address,
		                            &address_length, 0);
		if (unlikely(client_socket < 0)) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) return;

			panic("accept4: %s", strerror(errno));
		}

		metrics_server_thread_spawn(client_socket);
	}
}

static void
on_client_socket_epoll_event(struct epoll_event *evt)
{
	assert(evt);

	struct http_session *session = evt->data.ptr;
	assert(session);

	listener_thread_unregister_http_session(session);

	switch (session->state) {
	case HTTP_SESSION_RECEIVE_REQUEST_BLOCKED:
		assert((evt->events & EPOLLIN) == EPOLLIN);
		on_client_request_receiving(session);
		break;
	case HTTP_SESSION_SEND_RESPONSE_HEADER_BLOCKED:
		assert((evt->events & EPOLLOUT) == EPOLLOUT);
		on_client_response_header_sending(session);
		break;
	case HTTP_SESSION_SEND_RESPONSE_BODY_BLOCKED:
		assert((evt->events & EPOLLOUT) == EPOLLOUT);
		on_client_response_body_sending(session);
		break;
	default:
		panic("Invalid HTTP Session State");
	}
}

/**
 * @brief Request routing function
 * @param req_handle used by eRPC internal, it is used to send out the response packet
 * @param req_type the type of the request. Each function has a unique reqest type id
 * @param msg the payload of the rpc request. It is the input parameter fot the function
 * @param size the size of the msg
 */
void edf_interrupt_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port) {

    if (first_request_comming == false){
        t_start = time(NULL);
        first_request_comming = true;
    }

    uint8_t kMsgSize = 16;

    struct tenant *tenant = tenant_database_find_by_port(port);
    assert(tenant != NULL);
    struct route *route = http_router_match_request_type(&tenant->router, req_type);
    if (route == NULL) {
        debuglog("Did not match any routes\n");
		dispatcher_send_response(req_handle, DIPATCH_ROUNTE_ERROR, strlen(DIPATCH_ROUNTE_ERROR)); 
        return;
    }

    /*
     * Perform admissions control.
     * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
     * TODO: Consider providing a Retry-After header
     */
    uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
    if (work_admitted == 0) {
        dispatcher_send_response(req_handle, WORK_ADMITTED_ERROR, strlen(WORK_ADMITTED_ERROR));
        return;
    }

    total_requests++;
    /* Allocate a Sandbox */
    //session->state          = HTTP_SESSION_EXECUTING;
    struct sandbox *sandbox = sandbox_alloc(route->module, NULL, route, tenant, work_admitted, req_handle, dispatcher_thread_idx);
    if (unlikely(sandbox == NULL)) {
        debuglog("Failed to allocate sandbox\n");
		dispatcher_send_response(req_handle, SANDBOX_ALLOCATION_ERROR, strlen(SANDBOX_ALLOCATION_ERROR));
        return;
    }

    /* Reset estimated execution time and relative deadline for exponential service time simulation */
    if (runtime_exponential_service_time_simulation_enabled) {
	int exp_num = atoi((const char *)msg);
	if (exp_num == 1) {
		sandbox->estimated_cost = base_simulated_service_time;
	} else {
		sandbox->estimated_cost = base_simulated_service_time * exp_num * (1 - LOSS_PERCENTAGE);
	}
	sandbox->relative_deadline = 10 * sandbox->estimated_cost;
	sandbox->absolute_deadline = sandbox->timestamp_of.allocation + sandbox->relative_deadline;
    }

    /* copy the received data since it will be released by erpc */
    sandbox->rpc_request_body = malloc(size);
    if (!sandbox->rpc_request_body) {
        panic("malloc request body failed\n");
    }

    memcpy(sandbox->rpc_request_body, msg, size);
    sandbox->rpc_request_body_size = size;

    uint64_t min_waiting_serving_time = UINT64_MAX;
    int thread_id = 0; /* This thread can server the request by waiting for a while */
    int candidate_thread_with_interrupt = -1; /* This thread can server the request immediately by interrupting 
					         the current one */

    next_loop_start_index++;
    if (next_loop_start_index == runtime_worker_group_size) {
        next_loop_start_index = 0;
    }

    //uint64_t waiting_times[3] = {0};
    for (uint32_t i = next_loop_start_index; i < next_loop_start_index + runtime_worker_group_size; ++i) {
	int true_idx = i % runtime_worker_group_size;
        bool need_interrupt;
        uint64_t waiting_serving_time = local_runqueue_try_add_index(worker_list[true_idx], sandbox, &need_interrupt);
	/*waiting_times[true_idx] = waiting_serving_time;
	if (waiting_times[true_idx] != 0) {
		mem_log("listener %d %d queue:\n", dispatcher_thread_idx, worker_list[true_idx]);
		local_runqueue_print_in_order(worker_list[true_idx]);	
	}*/
	/* The local queue is empty, the worker is idle, can be served this request immediately 
         * without interrupting 
         */
        if (waiting_serving_time == 0 && need_interrupt == false) {
            local_runqueue_add_index(worker_list[true_idx], sandbox);
	    //mem_log("listener %d %d is idle, choose it\n", dispatcher_thread_idx, worker_list[true_idx]);  
            return;
        } else if (waiting_serving_time == 0 && need_interrupt == true) {//The worker can serve the request immediately
									// by interrupting the current one
            /* We already have a candidate worker, continue to find a 
             * better worker without needing interrupt or a worker that has the minimum total amount of work
             */
            if (candidate_thread_with_interrupt != -1) {
		if (worker_queuing_cost[worker_list[true_idx]] < worker_queuing_cost[candidate_thread_with_interrupt]) {
			candidate_thread_with_interrupt = worker_list[true_idx];
		}
            } else {
                candidate_thread_with_interrupt = worker_list[true_idx];
            }
        } else if (min_waiting_serving_time > waiting_serving_time) {
            min_waiting_serving_time = waiting_serving_time;
            thread_id = worker_list[true_idx];	
        } 
    } 
	
    if (candidate_thread_with_interrupt != -1) {
        //urgent_request[candidate_thread_with_interrupt] = sandbox;
        local_runqueue_add_index(candidate_thread_with_interrupt, sandbox);
	//mem_log("listener %d %d can be interrupted immediately for req deadline %lu\n", dispatcher_thread_idx, 
	//	candidate_thread_with_interrupt, sandbox->absolute_deadline);
	
        preempt_worker(candidate_thread_with_interrupt);
	dispatcher_try_interrupts++;
    } else {
        local_runqueue_add_index(thread_id, sandbox);
	/*mem_log("listener %d %d has the min waiting time for req deadline %lu. 0:%lu 1:%lu 2:%lu\n", 
		 dispatcher_thread_idx, thread_id, sandbox->absolute_deadline, waiting_times[0], waiting_times[1], waiting_times[2]);
	local_runqueue_print_in_order(thread_id);
	int fake_id = thread_id % runtime_worker_group_size;
	for (int i = 0; i < runtime_worker_group_size;  i++) {
		if (fake_id != i) {
			mem_log("listener %d %d queue:\n", dispatcher_thread_idx, worker_list[i]);
			local_runqueue_print_in_order(worker_list[i]);
		}
	}*/
    }
}

/**
 * @brief Request routing function
 * @param req_handle used by eRPC internal, it is used to send out the response packet
 * @param req_type the type of the request. Each function has a unique reqest type id
 * @param msg the payload of the rpc request. It is the input parameter fot the function
 * @param size the size of the msg
 */
void darc_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port) {
	if (first_request_comming == false){
                t_start = time(NULL);
                first_request_comming = true;
        }

        uint8_t kMsgSize = 16;
        //TODO: rpc_id is hardcode now

        struct tenant *tenant = tenant_database_find_by_port(port);
        assert(tenant != NULL);
        struct route *route = http_router_match_request_type(&tenant->router, req_type);
        if (route == NULL) {
                debuglog("Did not match any routes\n");
                dispatcher_send_response(req_handle, DIPATCH_ROUNTE_ERROR, strlen(DIPATCH_ROUNTE_ERROR));
                return;
        }

        /*
         * Perform admissions control.
         * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
         * TODO: Consider providing a Retry-After header
         */
        uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
        if (work_admitted == 0) {
                dispatcher_send_response(req_handle, WORK_ADMITTED_ERROR, strlen(WORK_ADMITTED_ERROR));
                return;
        }

	total_requests++;
        /* Allocate a Sandbox */
        //session->state          = HTTP_SESSION_EXECUTING;
        struct sandbox *sandbox = sandbox_alloc(route->module, NULL, route, tenant, work_admitted, req_handle, dispatcher_thread_idx);
        if (unlikely(sandbox == NULL)) {
                debuglog("Failed to allocate sandbox\n");
                dispatcher_send_response(req_handle, SANDBOX_ALLOCATION_ERROR, strlen(SANDBOX_ALLOCATION_ERROR));
                return;
        }

        /* copy the received data since it will be released by erpc */
        sandbox->rpc_request_body = malloc(size);
        if (!sandbox->rpc_request_body) {
                panic("malloc request body failed\n");
        }

        memcpy(sandbox->rpc_request_body, msg, size);
        sandbox->rpc_request_body_size = size;
	push_to_rqueue(sandbox, request_type_queue[req_type - 1], __getcycles());
	global_queue_length++;
        if (global_queue_length > max_queue_length) {
                max_queue_length = global_queue_length;
        }

}


/**
 * @brief Request routing function
 * @param req_handle used by eRPC internal, it is used to send out the response packet
 * @param req_type the type of the request. Each function has a unique reqest type id
 * @param msg the payload of the rpc request. It is the input parameter fot the function
 * @param size the size of the msg
 */
void shinjuku_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port) {
	if (first_request_comming == false){
                t_start = time(NULL);
                first_request_comming = true;
        }

        uint8_t kMsgSize = 16;
        //TODO: rpc_id is hardcode now

        struct tenant *tenant = tenant_database_find_by_port(port);
        assert(tenant != NULL);
        struct route *route = http_router_match_request_type(&tenant->router, req_type);
        if (route == NULL) {
                debuglog("Did not match any routes\n");
                dispatcher_send_response(req_handle, DIPATCH_ROUNTE_ERROR, strlen(DIPATCH_ROUNTE_ERROR));
                return;
        }

        /*
         * Perform admissions control.
         * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
         * TODO: Consider providing a Retry-After header
         */
        uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
        if (work_admitted == 0) {
                dispatcher_send_response(req_handle, WORK_ADMITTED_ERROR, strlen(WORK_ADMITTED_ERROR));
                return;
        }

	total_requests++;
        /* Allocate a Sandbox */
        //session->state          = HTTP_SESSION_EXECUTING;
        struct sandbox *sandbox = sandbox_alloc(route->module, NULL, route, tenant, work_admitted, req_handle, dispatcher_thread_idx);
        if (unlikely(sandbox == NULL)) {
                debuglog("Failed to allocate sandbox\n");
                dispatcher_send_response(req_handle, SANDBOX_ALLOCATION_ERROR, strlen(SANDBOX_ALLOCATION_ERROR));
                return;
        }

	/* Reset estimated execution time and relative deadline for exponential service time simulation */
    	if (runtime_exponential_service_time_simulation_enabled) {
		int exp_num = atoi((const char *)msg);
        	if (exp_num == 1) {
                	sandbox->estimated_cost = base_simulated_service_time;
        	} else {
                	sandbox->estimated_cost = base_simulated_service_time * exp_num * (1 - LOSS_PERCENTAGE);
        	}
        	sandbox->relative_deadline = 10 * sandbox->estimated_cost;
        	sandbox->absolute_deadline = sandbox->timestamp_of.allocation + sandbox->relative_deadline;
    	}

        /* copy the received data since it will be released by erpc */
        sandbox->rpc_request_body = malloc(size);
        if (!sandbox->rpc_request_body) {
                panic("malloc request body failed\n");
        }

        memcpy(sandbox->rpc_request_body, msg, size);
        sandbox->rpc_request_body_size = size;
	insertrear(request_type_deque[req_type -1], sandbox, __getcycles());
	global_queue_length++;
	if (global_queue_length > max_queue_length) {
		max_queue_length = global_queue_length;
	}
}

void drain_queue(struct request_typed_queue *rtype) {
    assert(rtype != NULL);
    /* queue is not empty and there is free workers */
    while (rtype->rqueue_head > rtype->rqueue_tail && free_workers[dispatcher_thread_idx] > 0) {
        uint32_t worker_id = MAX_WORKERS + 1;
        // Lookup for a core reserved to this type's group
        for (uint32_t i = 0; i < rtype->n_resas; ++i) {
            uint32_t candidate = rtype->res_workers[i];
            if ((1 << candidate) & free_workers[dispatcher_thread_idx]) {
                worker_id = candidate;
                //printf("Using reserved core %u\n", worker_list[worker_id]);
                break;
            }
        }
        // Otherwise attempt to steal worker
        if (worker_id == MAX_WORKERS + 1) {
            for (unsigned int i = 0; i < rtype->n_stealable; ++i) {
                uint32_t candidate = rtype->stealable_workers[i];
                if ((1 << candidate) & free_workers[dispatcher_thread_idx]) {
                    worker_id = candidate;
                    //printf("Stealing core %u\n", worker_list[worker_id]);
                    break;
                }
            }
        }
        // No peer found
        if (worker_id == MAX_WORKERS + 1) {
            return; 
        }

        // Dispatch
        struct sandbox *sandbox = rtype->rqueue[rtype->rqueue_tail & (RQUEUE_LEN - 1)];
	    //add sandbox to worker's local queue
	    local_runqueue_add_index(worker_list[worker_id], sandbox);
	    global_queue_length--;
	    rtype->rqueue_tail++;
	    atomic_fetch_xor(&free_workers[dispatcher_thread_idx], 1 << worker_id);
    }

}

/* Return selected sandbox and selected queue type, not pop it out */
struct sandbox * shinjuku_peek_selected_sandbox(int *selected_queue_idx) {
    float highest_priority = 0;
    *selected_queue_idx = -1;
    struct sandbox *selected_sandbox = NULL;

    for (uint32_t i = 0; i < n_rtypes; ++i) {
        if (isEmpty(request_type_deque[i]))
            continue;
        uint64_t ts = 0;
	/* get the front sandbox of the deque */
        struct sandbox *sandbox = getFront(request_type_deque[i], &ts);
        assert(sandbox != NULL);
        uint64_t dt = __getcycles() - ts;
        float priority = (float)dt / (float)sandbox->relative_deadline;
        if (priority > highest_priority) {
            highest_priority = priority;
            *selected_queue_idx = i;
            selected_sandbox = sandbox;
        }
	    
    }

    return selected_sandbox;

}

void shinjuku_dequeue_selected_sandbox(int selected_queue_type) {
    assert(selected_queue_type >= 0 && selected_queue_type < MAX_REQUEST_TYPE);
    deletefront(request_type_deque[selected_queue_type]);
}

/* Return selected sandbox and pop it out */
struct sandbox * shinjuku_select_sandbox() {
    float highest_priority = -1;
    int selected_queue_idx = -1;
    struct sandbox *selected_sandbox = NULL;

    for (uint32_t i = 0; i < n_rtypes; ++i) {
        if (isEmpty(request_type_deque[i]))
            continue;
        uint64_t ts = 0;
        /* get the front sandbox of the deque */
        struct sandbox *sandbox = getFront(request_type_deque[i], &ts);
        assert(sandbox != NULL);
        uint64_t dt = __getcycles() - ts;
        float priority = (float)dt / (float)sandbox->relative_deadline;
        if (priority > highest_priority) {
            highest_priority = priority;
            selected_queue_idx = i;
            selected_sandbox = sandbox;
        }
    }

    if (selected_sandbox != NULL) {
        deletefront(request_type_deque[selected_queue_idx]);
    }

    return selected_sandbox;

}

void darc_dispatch() {
	for (uint32_t i = 0; i < n_rtypes; ++i) {
	    // request_type_queue is a sorted queue, so the loop will dispatch packets from
	    // the earliest deadline to least earliest deadline
            if (request_type_queue[i]->rqueue_head > request_type_queue[i]->rqueue_tail) {
            	drain_queue(request_type_queue[i]);
            }
        }
}

void shinjuku_dispatch_different_core() {
    for (uint32_t i = 0; i < runtime_worker_group_size; ++i) {
        /* move all preempted sandboxes from worker to dispatcher's typed queue */
        struct request_fifo_queue * preempted_queue = worker_preempted_queue[worker_list[i]];
        assert(preempted_queue != NULL);
        uint64_t tsc = 0;
        struct sandbox * preempted_sandbox = pop_worker_preempted_queue(worker_list[i], preempted_queue, &tsc);
        while(preempted_sandbox != NULL) {
            uint8_t req_type = preempted_sandbox->route->request_type;
            insertfront(request_type_deque[req_type - 1], preempted_sandbox, tsc);
            preempted_sandbox = pop_worker_preempted_queue(worker_list[i], preempted_queue, &tsc);
        }

        /* core is idle */
        if ((1 << i) & free_workers[dispatcher_thread_idx]) {
            struct sandbox *sandbox = shinjuku_select_sandbox();
            if (!sandbox) return; // queue is empty

	    sandbox->start_ts = __getcycles();
            local_runqueue_add_index(worker_list[i], sandbox);
            atomic_fetch_xor(&free_workers[dispatcher_thread_idx], 1 << i);
        } else { // core is busy
            //check if the current sandbox is running longer than the specified time duration
            struct sandbox *current = current_sandboxes[worker_list[i]];
            if (!current) continue; //In case that worker thread hasn't call current_sandbox_set to set the current sandbox
            uint64_t elapsed_cycles = (__getcycles() - current->start_ts);
            if (elapsed_cycles >= shinjuku_interrupt_interval && (current->state == SANDBOX_RUNNING_USER || current->state == SANDBOX_RUNNING_SYS)) {
                struct sandbox *sandbox = shinjuku_select_sandbox();
                if (!sandbox) return; // queue is empty

                //preempt the current sandbox and put it back the typed queue, select a new one to send to it
                sandbox->start_ts = __getcycles();
		local_runqueue_add_index(worker_list[i], sandbox);
                //preempt worker
		dispatcher_try_interrupts++;
                preempt_worker(worker_list[i]);
            }
        }

    }

}

void shinjuku_dispatch() {
    next_loop_start_index++;
    if (next_loop_start_index == runtime_worker_group_size) {
    	next_loop_start_index = 0;	
    }

    for (uint32_t i = next_loop_start_index; i < next_loop_start_index + runtime_worker_group_size; ++i) {
	int true_idx = i % runtime_worker_group_size;
        /* move all preempted sandboxes from worker to dispatcher's typed queue */
        struct request_fifo_queue * preempted_queue = worker_preempted_queue[worker_list[true_idx]];
	assert(preempted_queue != NULL);
	uint64_t tsc = 0;
        struct sandbox * preempted_sandbox = pop_worker_preempted_queue(worker_list[true_idx], preempted_queue, &tsc);
	while(preempted_sandbox != NULL) {
	    uint8_t req_type = preempted_sandbox->route->request_type; 
	    insertfront(request_type_deque[req_type - 1], preempted_sandbox, tsc);
	    global_queue_length++;
	    //insertrear(request_type_deque[req_type - 1], preempted_sandbox, tsc);
	    preempted_sandbox = pop_worker_preempted_queue(worker_list[true_idx], preempted_queue, &tsc);     
	} 
        /* core is idle */
        //if ((1 << i) & free_workers[dispatcher_thread_idx]) {
        if (local_runqueue_get_length_index(worker_list[true_idx]) == 0) {
            struct sandbox *sandbox = shinjuku_select_sandbox();
            if (!sandbox) return; // queue is empty
            while (sandbox->global_worker_thread_idx != -1) {
		sandbox->start_ts = __getcycles();
                local_runqueue_add_index(sandbox->global_worker_thread_idx, sandbox);
		global_queue_length--;
		worker_old_sandbox[worker_list[true_idx]]++;
                sandbox = shinjuku_select_sandbox();
                if (!sandbox) return;
            }
            
	    sandbox->start_ts = __getcycles();    
            local_runqueue_add_index(worker_list[true_idx], sandbox);
	    global_queue_length--;
            worker_new_sandbox[worker_list[true_idx]]++;
            atomic_fetch_xor(&free_workers[dispatcher_thread_idx], 1 << true_idx);
        } else { // core is busy
            //check if the current sandbox is running longer than the specified time duration
            struct sandbox *current = current_sandboxes[worker_list[true_idx]];
            if (!current) continue; //In case that worker thread hasn't call current_sandbox_set to set the current sandbox
            uint64_t elapsed_cycles = (__getcycles() - current->start_ts);
	    if (elapsed_cycles >= shinjuku_interrupt_interval && (current->state == SANDBOX_RUNNING_USER || current->state == SANDBOX_RUNNING_SYS)) {
                struct sandbox *sandbox = shinjuku_select_sandbox();
                if (!sandbox) return; // queue is empty

                while (sandbox->global_worker_thread_idx != -1) {
		    sandbox->start_ts = __getcycles();
                    local_runqueue_add_index(sandbox->global_worker_thread_idx, sandbox);
		    global_queue_length--;
		    worker_old_sandbox[worker_list[true_idx]]++;
                    sandbox = shinjuku_select_sandbox();
                    if (!sandbox) return;
                }

                //preempt the current sandbox and put it back the typed queue, select a new one to send to it
		sandbox->start_ts = __getcycles();
                local_runqueue_add_index(worker_list[true_idx], sandbox);
		global_queue_length--;
		worker_new_sandbox[worker_list[true_idx]]++;
                //preempt worker
                preempt_worker(worker_list[true_idx]);
		dispatcher_try_interrupts++;
            }
        }
            
    }
}

void dispatcher_send_response(void *req_handle, char* msg, size_t msg_len) {
	erpc_req_response_enqueue(dispatcher_thread_idx, req_handle, msg, msg_len, 1);   
}
/**
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and
 * pushes the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * listener_thread_epoll_file_descriptor - the epoll file descriptor
 *
 */
void *
listener_thread_main(void *dummy)
{
	is_listener = true;
	shinjuku_interrupt_interval = INTERRUPT_INTERVAL * runtime_processor_speed_MHz;
	base_simulated_service_time = BASE_SERVICE_TIME * runtime_processor_speed_MHz;

	/* Initialize memory logging, set 1M memory for logging */
        mem_log_init2(1024*1024*1024, sandbox_perf_log);

	/* Unmask SIGINT signals */
    	software_interrupt_unmask_signal(SIGINT);

	/* Index was passed via argument */
    	dispatcher_thread_idx = *(int *)dummy;

	/* init typed queue */
	typed_queue_init();

	/* calucate the worker start and end id for this listener */
	worker_start_id = dispatcher_thread_idx * runtime_worker_group_size;
	worker_end_id = worker_start_id + runtime_worker_group_size;
	printf("listener %d worker_start_id %d worker_end_id %d\n", dispatcher_thread_idx, worker_start_id, worker_end_id - 1);

	int index = 0;
	for (uint32_t i = worker_start_id; i < worker_end_id; i++) {
		worker_list[index] = i;
		index++;
	}	

	free_workers[dispatcher_thread_idx] = __builtin_powi(2, runtime_worker_group_size) - 1;	

	printf("free_workers is %u\n", free_workers[dispatcher_thread_idx]);

	struct epoll_event epoll_events[RUNTIME_MAX_EPOLL_EVENTS];

	metrics_server_init();
	listener_thread_register_metrics_server();

	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 2);
	pthread_setschedprio(pthread_self(), -20);

	erpc_start(NULL, dispatcher_thread_idx, NULL, 0);

	if (dispatcher == DISPATCHER_EDF_INTERRUPT) {
        	printf("edf_interrupt....\n");	
		while (!pthread_stop) {
			erpc_run_event_loop(dispatcher_thread_idx, 1000);
		}
	} else if (dispatcher == DISPATCHER_DARC) {
        	printf("darc....\n");
		while (!pthread_stop) {
			erpc_run_event_loop_once(dispatcher_thread_idx); // get a group of packets from the NIC and enqueue them to the typed queue
			darc_dispatch(); //dispatch packets
		}
	} else if (dispatcher == DISPATCHER_SHINJUKU) {
        	printf("shinjuku....\n");
		while (!pthread_stop) {
            		erpc_run_event_loop_once(dispatcher_thread_idx); // get a group of packets from the NIC and enqueue them to the typed queue
            		shinjuku_dispatch(); //dispatch packets
        	}
	}

	/* won't go to the following implementaion */
	while (!pthread_stop) {
		printf("pthread_stop is false\n");
		/* Block indefinitely on the epoll file descriptor, waiting on up to a max number of events */
		int descriptor_count = epoll_wait(listener_thread_epoll_file_descriptor, epoll_events,
		                                  RUNTIME_MAX_EPOLL_EVENTS, -1);
		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic("epoll_wait: %s", strerror(errno));
		}

		/* Assumption: Because epoll_wait is set to not timeout, we should always have descriptors here */
		assert(descriptor_count > 0);

		for (int i = 0; i < descriptor_count; i++) {
			panic_on_epoll_error(&epoll_events[i]);

			enum epoll_tag tag = *(enum epoll_tag *)epoll_events[i].data.ptr;

			switch (tag) {
			case EPOLL_TAG_TENANT_SERVER_SOCKET:
				on_tenant_socket_epoll_event(&epoll_events[i]);
				break;
			case EPOLL_TAG_HTTP_SESSION_CLIENT_SOCKET:
				on_client_socket_epoll_event(&epoll_events[i]);
				break;
			case EPOLL_TAG_METRICS_SERVER_SOCKET:
				on_metrics_server_epoll_event(&epoll_events[i]);
				break;
			default:
				panic("Unknown epoll type!");
			}
		}
	}
	return NULL;
	//panic("Listener thread unexpectedly broke loop\n");
}
