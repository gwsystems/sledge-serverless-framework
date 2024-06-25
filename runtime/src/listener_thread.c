#include <stdint.h>
#include <unistd.h>

#include "admissions_control.h"
#include "traffic_control.h"
#include "arch/getcycles.h"
#include "execution_regression.h"
#include "global_request_scheduler.h"
#include "http_session_perf_log.h"
#include "listener_thread.h"
#include "metrics_server.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "sandbox_perf_log.h"
#include "tcp_session.h"
#include "tenant.h"
#include "tenant_functions.h"

#include "sandbox_perf_log.h"
#include "http_session_perf_log.h"
#include "ck_ring.h"
#include "priority_queue.h"
#include "global_request_scheduler_mtdbf.h"
#include "sandbox_set_as_error.h"

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

pthread_t listener_thread_id;

struct comm_with_worker *comm_from_workers, *comm_to_workers;
extern lock_t global_lock;

static struct sandbox_metadata *global_sandbox_meta = NULL;

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread_initialize(void)
{
	printf("Starting listener thread\n");

	comm_from_workers       = calloc(runtime_worker_threads_count, sizeof(struct comm_with_worker));
	comm_to_workers         = calloc(runtime_worker_threads_count, sizeof(struct comm_with_worker));
	comm_with_workers_init(comm_from_workers);
	comm_with_workers_init(comm_to_workers);

	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(LISTENER_THREAD_CORE_ID, &cs);

	/* Setup epoll */
	listener_thread_epoll_file_descriptor = epoll_create1(0);
	assert(listener_thread_epoll_file_descriptor >= 0);

	int ret = pthread_create(&listener_thread_id, NULL, listener_thread_main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread_id, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	if (geteuid() != 0) ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	printf("\tListener core thread: %lx\n", listener_thread_id);
}

/**
 * @brief Registers a serverless http session on the listener thread's epoll descriptor
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
 * @brief Registers a serverless http session on the listener thread's epoll descriptor
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
check_messages_from_workers()
{
#ifdef TRAFFIC_CONTROL
	assert(comm_from_workers);
	assert(comm_to_workers);

	for (int worker_idx = 0; worker_idx < runtime_worker_threads_count; worker_idx++) {
		struct message           new_message = { 0 };
		struct comm_with_worker *cfw         = &comm_from_workers[worker_idx];
		struct comm_with_worker *ctw         = &comm_to_workers[worker_idx];
		assert(cfw);
		assert(ctw);
		assert(cfw->worker_idx == worker_idx);
		assert(ctw->worker_idx == worker_idx);

		const uint64_t now = __getcycles();

		while (ck_ring_dequeue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
			assert(new_message.sender_worker_idx == worker_idx);
			assert(new_message.sandbox_meta);
			
			struct sandbox_metadata *sandbox_meta = new_message.sandbox_meta;
			assert(new_message.sandbox_id == sandbox_meta->id);

			struct tenant *tenant = sandbox_meta->tenant;
			struct route *route = sandbox_meta->route;
			const uint64_t absolute_deadline = sandbox_meta->absolute_deadline;		
			const uint64_t allocation_timestamp = sandbox_meta->allocation_timestamp;
			
			assert(tenant);
			assert(route);
			assert(absolute_deadline > 0);
			assert(allocation_timestamp > 0);

			sandbox_meta->exceeded_estimation = new_message.exceeded_estimation;
			sandbox_meta->state = new_message.state;
			sandbox_meta->owned_worker_idx = new_message.sender_worker_idx;

			switch (new_message.message_type)
			{
			case MESSAGE_CFW_PULLED_NEW_SANDBOX: {
				assert(sandbox_meta->state == SANDBOX_RUNNABLE);

				if (sandbox_meta->terminated) continue;
				
				if (sandbox_meta->pq_idx_in_tenant_queue) {
					assert(sandbox_meta->global_queue_type == 2);
					assert(sandbox_meta->tenant_queue == tenant->global_sandbox_metas);
					priority_queue_delete_by_idx_nolock(tenant->global_sandbox_metas,
											sandbox_meta,sandbox_meta->pq_idx_in_tenant_queue);
					if(unlikely(priority_queue_enqueue_nolock(tenant->local_sandbox_metas, sandbox_meta))){
						panic("Failed to add sandbox_meta to tenant metadata queue");
					}
					sandbox_meta->tenant_queue = tenant->local_sandbox_metas;
				}
				break;
			}
			case MESSAGE_CFW_DELETE_SANDBOX: {
				assert(sandbox_meta->state == SANDBOX_RETURNED || sandbox_meta->state == SANDBOX_ERROR);
				// assert(new_message.adjustment > 0);
				assert(sandbox_meta->terminated || sandbox_meta->remaining_exec > 0);
				if (!sandbox_meta->terminated){					
					assert(sandbox_meta->worker_id_virt >= 0);
					// assert(sandbox_meta->remaining_exec == new_message.adjustment);
					void *global_dbf = global_virt_worker_dbfs[sandbox_meta->worker_id_virt];

					dbf_list_reduce_demand(sandbox_meta, sandbox_meta->remaining_exec + sandbox_meta->extra_slack, true);
					sandbox_meta->demand_node = NULL;
					// sandbox_meta->extra_slack = 0;
		
					// sandbox_meta->remaining_exec -= new_message.adjustment;
					assert(sandbox_meta->remaining_exec == new_message.remaining_exec);
				}
				// assert(sandbox_meta->remaining_exec == 0);

				if (sandbox_meta->trs_job_node) tenant_reduce_guaranteed_job_demand(tenant, sandbox_meta->remaining_exec, sandbox_meta);

				// if (sandbox_meta->global_queue_type == 2 && sandbox_meta->state == SANDBOX_RETURNED) sandbox_meta->total_be_exec_cycles += new_message.adjustment;
				if (sandbox_meta->total_be_exec_cycles > 0) tenant_force_add_job_as_best(tenant, now, sandbox_meta->total_be_exec_cycles);

				sandbox_meta->terminated = true;
				break;
			}
			case MESSAGE_CFW_REDUCE_DEMAND: {
				assert(new_message.adjustment > 0);
				if (sandbox_meta->global_queue_type == 2) {
					sandbox_meta->total_be_exec_cycles += new_message.adjustment;
					// tenant_force_add_job_as_best(tenant, now, sandbox_meta->total_be_exec_cycles);
				}

				if (sandbox_meta->terminated) break;
				
				assert(sandbox_meta->demand_node);
				assert(sandbox_meta->worker_id_virt >= 0);
				void *global_dbf = global_virt_worker_dbfs[sandbox_meta->worker_id_virt];
				dbf_list_reduce_demand(sandbox_meta, new_message.adjustment, false);
	
				sandbox_meta->remaining_exec -= new_message.adjustment;
				assert(sandbox_meta->remaining_exec == new_message.remaining_exec);

				if (sandbox_meta->remaining_exec == 0 && sandbox_meta->extra_slack < runtime_quantum) {
					dbf_list_reduce_demand(sandbox_meta, sandbox_meta->extra_slack, true);
					sandbox_meta->extra_slack = 0;
					sandbox_meta->demand_node = NULL;
				}
				break;
			}
			case MESSAGE_CFW_EXTRA_DEMAND_REQUEST: {
				assert(USING_TRY_LOCAL_EXTRA);
				assert(new_message.exceeded_estimation);
				assert(new_message.adjustment == runtime_quantum);

				if (sandbox_meta->terminated) break;

				assert(sandbox_meta->remaining_exec == 0);
				if (absolute_deadline <= now) {
					assert(sandbox_meta->error_code == 0);
					if (sandbox_meta->extra_slack > 0) {
						assert(sandbox_meta->demand_node);
						dbf_list_reduce_demand(sandbox_meta, sandbox_meta->extra_slack, true);
						sandbox_meta->demand_node = NULL;
						// sandbox_meta->extra_slack = 0;
					}
					assert(sandbox_meta->demand_node == NULL);
					sandbox_meta->error_code = 4081;
					sandbox_meta->terminated = true;
					break;
				}

				if (sandbox_meta->global_queue_type == 1) {
					assert(sandbox_meta->trs_job_node);
					assert(sandbox_meta->trs_job_node->sandbox_meta);
					sandbox_meta->trs_job_node->sandbox_meta = NULL;
					sandbox_meta->trs_job_node = NULL;
				}

				int 	 return_code   = 0;
				int 	 worker_id_v   = -1;
				uint64_t work_admitted = 0;
				
				if (sandbox_meta->extra_slack >= runtime_quantum) {
					bool tenant_can_admit = tenant_try_add_job_as_guaranteed(tenant, now, runtime_quantum, sandbox_meta);
					return_code = tenant_can_admit ? 1 : 2;

					worker_id_v = sandbox_meta->worker_id_virt;
					work_admitted = 1;

					sandbox_meta->extra_slack -= runtime_quantum;
				} else {
					assert(sandbox_meta->demand_node == NULL);
					assert(sandbox_meta->extra_slack == 0);
					work_admitted = traffic_control_decide(sandbox_meta, now, runtime_quantum, &return_code, &worker_id_v);
				}

				if (work_admitted == 0) {
					// debuglog("No global supply left");
					assert(return_code == 4295 || return_code == 4296);
					assert(sandbox_meta->demand_node == NULL);
					assert(sandbox_meta->extra_slack == 0);
					assert(sandbox_meta->error_code == 0);
					sandbox_meta->error_code = return_code;
					sandbox_meta->terminated = true;
					break;
				}

				assert(worker_id_v >= 0);
				sandbox_meta->remaining_exec = runtime_quantum;
				sandbox_meta->worker_id_virt = worker_id_v;

				// TODO: Fix the BE budget calculation for when promote/demote happens
				if (sandbox_meta->global_queue_type == 2 && return_code == 1) {
					assert(sandbox_meta->pq_idx_in_tenant_queue >= 1);
					priority_queue_delete_by_idx_nolock(tenant->local_sandbox_metas,
											sandbox_meta,sandbox_meta->pq_idx_in_tenant_queue);
					sandbox_meta->tenant_queue = NULL;
					// printf("promote!\n");
				} else if (sandbox_meta->global_queue_type == 1 && return_code == 2) {
					assert(sandbox_meta->pq_idx_in_tenant_queue == 0);
					if(unlikely(priority_queue_enqueue_nolock(tenant->local_sandbox_metas, sandbox_meta))){
						panic("Failed to add sandbox_meta to tenant metadata queue");
					}
					sandbox_meta->tenant_queue = tenant->local_sandbox_metas;
					// printf("demote!\n");
				}
				sandbox_meta->global_queue_type = return_code;
				break;
			}
			case MESSAGE_CFW_WRITEBACK_PREEMPTION: {
				assert(USING_WRITEBACK_FOR_PREEMPTION);
				assert(USING_LOCAL_RUNQUEUE == false);
				assert (sandbox_meta->state == SANDBOX_PREEMPTED);

				struct sandbox  *preempted_sandbox = new_message.sandbox;
				assert(preempted_sandbox);
				assert(preempted_sandbox == sandbox_meta->sandbox_shadow);
				assert(preempted_sandbox->sandbox_meta == sandbox_meta);
				assert(preempted_sandbox->id == new_message.sandbox_id);
				assert(preempted_sandbox->state == SANDBOX_PREEMPTED);
				assert(preempted_sandbox->writeback_preemption_in_progress);
				assert(preempted_sandbox->absolute_deadline == absolute_deadline);
				assert(preempted_sandbox->response_code == 0);
				assert(preempted_sandbox->remaining_exec > 0);

				if (sandbox_meta->terminated) {
					assert(sandbox_meta->error_code > 0);
					assert(preempted_sandbox->response_code == 0);
					// preempted_sandbox->response_code = sandbox_meta->error_code;
					// break;
					// printf("terminated - %s\n", tenant->name);
				} else if (absolute_deadline < now + (!preempted_sandbox->exceeded_estimation ? preempted_sandbox->remaining_exec : 0)) {
					// printf("missed - %s\n", tenant->name);
		/*		// if (absolute_deadline < now + preempted_sandbox->remaining_execution_original) {
					assert(sandbox_meta->terminated == false);
					assert(sandbox_meta->remaining_exec == preempted_sandbox->remaining_exec);

					assert(sandbox_meta->worker_id_virt >= 0);
					void *global_dbf = global_virt_worker_dbfs[sandbox_meta->worker_id_virt];
					dbf_try_update_demand(global_dbf, allocation_timestamp,
										route->relative_deadline,
										absolute_deadline, sandbox_meta->remaining_exec,
										DBF_DELETE_EXISTING_DEMAND, NULL, NULL);

					sandbox_meta->remaining_exec -= new_message.adjustment;
					assert(sandbox_meta->remaining_exec == new_message.remaining_exec);

					if (sandbox_meta->trs_job_node) {
						assert(sandbox_meta->global_queue_type == 1);
						tenant_update_job_node(tenant, sandbox_meta->remaining_exec, TRS_REDUCE_EXISTING_DEMAND, sandbox_meta);
					}  
					
					if (sandbox_meta->global_queue_type == 2) {
						assert(tenant->trs.best_effort_cycles >= sandbox_meta->remaining_exec);
						tenant->trs.best_effort_cycles -= sandbox_meta->remaining_exec;
					}

					sandbox_meta->terminated = true;
					assert(preempted_sandbox->response_code == 0);
					preempted_sandbox->response_code = 4082;
					break; */
					assert(sandbox_meta->error_code == 0);
					if (sandbox_meta->remaining_exec + sandbox_meta->extra_slack > 0) {
						assert(sandbox_meta->demand_node);
						dbf_list_reduce_demand(sandbox_meta, sandbox_meta->remaining_exec + sandbox_meta->extra_slack, true);
						sandbox_meta->demand_node = NULL;
						// sandbox_meta->remaining_exec = 0;
						// sandbox_meta->extra_slack = 0;
					}
					assert(sandbox_meta->demand_node == NULL);
					sandbox_meta->error_code = 4082;
					sandbox_meta->terminated = true;
				}
				
				assert(sandbox_meta->terminated || sandbox_meta->remaining_exec == preempted_sandbox->remaining_exec);

				if (unlikely(global_request_scheduler_add(preempted_sandbox) == NULL)) {
					// TODO: REDUCE DBF, for now just panic!
					panic("Failed to add the preempted_sandbox to global queue\n");
				}
				preempted_sandbox->writeback_preemption_in_progress = false;
				
				break;
			}
			case MESSAGE_CFW_WRITEBACK_OVERSHOOT: {
				assert(USING_WRITEBACK_FOR_OVERSHOOT);
				assert (sandbox_meta->state == SANDBOX_PREEMPTED);

				struct sandbox  *preempted_sandbox = new_message.sandbox;
				assert(preempted_sandbox);
				assert(preempted_sandbox == sandbox_meta->sandbox_shadow);
				assert(preempted_sandbox->sandbox_meta == sandbox_meta);
				assert(preempted_sandbox->id == new_message.sandbox_id);
				assert(preempted_sandbox->sandbox_meta == sandbox_meta);
				assert(preempted_sandbox->state == SANDBOX_PREEMPTED);
				assert(preempted_sandbox->writeback_overshoot_in_progress);
				assert(preempted_sandbox->absolute_deadline == absolute_deadline);
				assert(preempted_sandbox->response_code == 0);
				assert(preempted_sandbox->remaining_exec == 0);
				assert(new_message.remaining_exec == 0);
				assert(sandbox_meta->remaining_exec == 0);
				assert(new_message.adjustment == runtime_quantum);		
				
				if (sandbox_meta->terminated) {
					assert(sandbox_meta->error_code > 0);
					preempted_sandbox->response_code = sandbox_meta->error_code;
					break;
				}

				if (absolute_deadline <= now) {
					sandbox_meta->terminated = true;
					preempted_sandbox->response_code = 4082;
					break;
				}

				if (sandbox_meta->global_queue_type == 1) {
					assert(sandbox_meta->trs_job_node);
					assert(sandbox_meta->trs_job_node->sandbox_meta);
					sandbox_meta->trs_job_node->sandbox_meta = NULL;
					sandbox_meta->trs_job_node = NULL;
				}
				
				int 	 return_code   = 0;
				int 	 worker_id_v   = -1;
				uint64_t work_admitted = 0;
				
				work_admitted = traffic_control_decide(sandbox_meta, now, runtime_quantum,
									&return_code, &worker_id_v);

				if (work_admitted == 0) {
					// debuglog("No global supply left");
					assert(return_code == 4290 || return_code == 4291);
					preempted_sandbox->response_code = return_code + 5;
					sandbox_meta->terminated = true;
					break;
				}

				assert(worker_id_v >= 0);
				sandbox_meta->remaining_exec = runtime_quantum;
				sandbox_meta->worker_id_virt = worker_id_v;
				if (sandbox_meta->global_queue_type == 2 && return_code == 1) {
					assert(sandbox_meta->pq_idx_in_tenant_queue >= 1);
					priority_queue_delete_by_idx_nolock(tenant->local_sandbox_metas,
											sandbox_meta,sandbox_meta->pq_idx_in_tenant_queue);
				} else if (sandbox_meta->global_queue_type == 1 && return_code == 2) {
					assert(sandbox_meta->pq_idx_in_tenant_queue == 0);
					if(unlikely(priority_queue_enqueue_nolock(tenant->local_sandbox_metas, sandbox_meta))){
						panic("Failed to add sandbox_meta to tenant metadata queue");
					}
				}
				sandbox_meta->global_queue_type = return_code;

				preempted_sandbox->remaining_exec = runtime_quantum;
				preempted_sandbox->writeback_overshoot_in_progress = false;

				if (unlikely(global_request_scheduler_add(preempted_sandbox) == NULL)){
					// TODO: REDUCE DBF, for now just panic!
					panic("Failed to add the preempted_sandbox to global queue\n");
				}
				break;
			}
			default:
				panic("Unknown message type received by the listener!");
				break;
			} // end switch 1

			if (sandbox_meta->terminated == false) continue;

			assert(sandbox_meta->demand_node == NULL);
			
			if (sandbox_meta->pq_idx_in_tenant_queue) {
				assert(sandbox_meta->global_queue_type == 2);
				assert(sandbox_meta->tenant_queue);
				priority_queue_delete_by_idx_nolock(sandbox_meta->tenant_queue,
										sandbox_meta,sandbox_meta->pq_idx_in_tenant_queue);
				sandbox_meta->tenant_queue = NULL;
			}

			switch (new_message.message_type) {
			case MESSAGE_CFW_EXTRA_DEMAND_REQUEST:
				if (sandbox_refs[new_message.sandbox_id % RUNTIME_MAX_ALIVE_SANDBOXES]){
					new_message.message_type = MESSAGE_CTW_SHED_CURRENT_JOB;
					if (!ck_ring_enqueue_spsc_message(&ctw->worker_ring, ctw->worker_ring_buffer, &new_message)) {
						panic("Ring buffer was full and enqueue has failed!")
					}
					pthread_kill(runtime_worker_threads[new_message.sender_worker_idx], SIGALRM);
				} else {
					// printf("already dead\n");
				}
				break;
			case MESSAGE_CFW_REDUCE_DEMAND:
				break;
			case MESSAGE_CFW_DELETE_SANDBOX:
				assert (sandbox_meta->state == SANDBOX_RETURNED || sandbox_meta->state == SANDBOX_ERROR);
				free(sandbox_meta);
				break;
			case MESSAGE_CFW_WRITEBACK_PREEMPTION:
			case MESSAGE_CFW_WRITEBACK_OVERSHOOT:
				// assert(preempted_sandbox);
				// assert(preempted_sandbox->id == sandbox_meta->id);
				// assert(preempted_sandbox->state == SANDBOX_PREEMPTED);

				// sandbox_set_as_error(preempted_sandbox, SANDBOX_PREEMPTED);
				// sandbox_free(preempted_sandbox);
				// free(sandbox_meta);
				break;
			default:
				panic ("Unknown message type!");
				break;
			} // end switch 2
			// memset(&new_message, 0, sizeof(new_message));
		} // end while
	} // end for

#endif
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

	/* Check if the route is accurate only when the URL is downloaded. Stop downloading if inaccurate. */
	if (session->route == NULL && strlen(session->http_request.full_url) > 0) {
		struct route *route = http_router_match_route(&session->tenant->router, session->http_request.full_url);
		if (route == NULL) {
			debuglog("Did not match any routes\n");
			session->state = HTTP_SESSION_EXECUTION_COMPLETE;
			http_session_set_response_header(session, 404);
			on_client_response_header_sending(session);
			return;
		}

		session->route = route;
	}

	if (rc == 0) {
#ifdef EXECUTION_REGRESSION
		if (!session->did_preprocessing) tenant_preprocess(session);
#endif
		on_client_request_received(session);
		return;
	} else if (rc == -EAGAIN) {
		/* session blocked and registered to epoll, so continue to next handle */
#ifdef EXECUTION_REGRESSION
		/* try tenant preprocessing if min 4k Bytes received */
		if (!session->did_preprocessing && session->http_request.body_length_read > 4096)
			tenant_preprocess(session);
#endif
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
	const uint64_t now = __getcycles();
	session->request_downloaded_timestamp = now;

	struct tenant *tenant = session->tenant;
	struct route  *route  = session->route;
	http_route_total_increment_request(&session->route->metrics);

	// uint64_t estimated_execution = route->execution_histogram.estimated_execution; // By defaulat this is half of deadline
	// uint64_t work_admitted       = 1;

	// struct route *route = http_router_match_route(&tenant->router, session->http_request.full_url);
	// if (route == NULL) {
	// 	debuglog("Route: %s did not match any routes\n", session->http_request.full_url);
	// 	session->state = HTTP_SESSION_EXECUTION_COMPLETE;
	// 	http_session_set_response_header(session, 404);
	// 	on_client_response_header_sending(session);
	// 	return;
	// }

	// session->route = route;
	// http_route_total_increment_request(&session->route->metrics);

#if defined TRAFFIC_CONTROL
	/*
	 * Admin control.
	 * If client sends a request to the route "/main", server prints all the DBF data.
	 * If client sends a request to the route "/terminator", server does cleanup and terminates.
	 */
	if (tenant->tcp_server.port == 55555) {
		if (strcmp(session->http_request.full_url, "/terminator") == 0) {
			printf("Terminating SLEdge now!\n");
			tenant_database_print_reservations();
			printf("\nGLOBAL DBF DEMANDS:\n");
			const int N_VIRT_WORKERS_DBF = USING_AGGREGATED_GLOBAL_DBF ? 1 : runtime_worker_threads_count;
			for (int i = 0; i < N_VIRT_WORKERS_DBF; i++)
			{
				printf("GL Worker #%d\n", i);
				dbf_print(global_virt_worker_dbfs[i], now);
			}
			
			// dbf_print(global_dbf, now);
			// printf("\nGLOBAL GUAR DBF DEMANDS:\n");
			printf("\nWorker #0 DBF DEMANDS:\n");
			dbf_print(global_worker_dbf, now);
			
			session->state = HTTP_SESSION_EXECUTION_COMPLETE;
			http_session_set_response_header(session, 500);
			on_client_response_header_sending(session);
			runtime_cleanup();
			exit(0);
		}

		if (strcmp(session->http_request.full_url, "/admin") == 0) {
			printf("Hello from Admin!\n");
			tenant_database_print_reservations();
			printf("\nGLOBAL DBF DEMANDS:\n");
			const int N_VIRT_WORKERS_DBF = USING_AGGREGATED_GLOBAL_DBF ? 1 : runtime_worker_threads_count;
			for (int i = 0; i < N_VIRT_WORKERS_DBF; i++)
			{
				printf("GL Worker #%d\n", i);
				dbf_print(global_virt_worker_dbfs[i], now);
			}
			
			// dbf_print(global_dbf, now);
			// printf("\nGLOBAL GUAR DBF DEMANDS:\n");
			printf("\nWorker #0 DBF DEMANDS:\n");
			dbf_print(global_worker_dbf, now);
			
			session->state = HTTP_SESSION_EXECUTION_COMPLETE;
			http_session_set_response_header(session, 500);
			on_client_response_header_sending(session);
			return;
		}
	}
#endif

	// const uint64_t sandbox_alloc_timestamp = __getcycles();
	const uint64_t absolute_deadline = now + route->relative_deadline;
	uint64_t estimated_execution     = route->execution_histogram.estimated_execution; // TODO: By default half of deadline
	uint64_t       work_admitted     = 1;
	int            return_code       = 0;
	int            worker_id_v       = -1;

#ifdef EXECUTION_REGRESSION
	estimated_execution = get_regression_prediction(session);
#endif

/*
 * Perform admissions control.
 * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
 */
#if defined ADMISSIONS_CONTROL
	const uint64_t admissions_estimate = admissions_control_calculate_estimate(estimated_execution,
	                                                                     route->relative_deadline);
	work_admitted                = admissions_control_decide(admissions_estimate);
#elif defined TRAFFIC_CONTROL
	if (global_sandbox_meta == NULL) global_sandbox_meta = malloc(sizeof(struct sandbox_metadata));

	assert(global_sandbox_meta);
	global_sandbox_meta->tenant = tenant;
	global_sandbox_meta->route = route;
	global_sandbox_meta->tenant_queue = NULL;
	global_sandbox_meta->sandbox_shadow = NULL;
	global_sandbox_meta->global_queue_type = 0;
	global_sandbox_meta->pq_idx_in_tenant_queue = 0;
	global_sandbox_meta->error_code = 0;
	global_sandbox_meta->exceeded_estimation = false;
	global_sandbox_meta->terminated = false;
	global_sandbox_meta->demand_node = NULL;
	global_sandbox_meta->trs_job_node = NULL;
	global_sandbox_meta->extra_slack = 0;
	global_sandbox_meta->total_be_exec_cycles = 0;
	global_sandbox_meta->owned_worker_idx = -2;

	global_sandbox_meta->allocation_timestamp = now;
	global_sandbox_meta->absolute_deadline = absolute_deadline;
	global_sandbox_meta->remaining_exec  = estimated_execution;
	
	work_admitted = traffic_control_decide(global_sandbox_meta, now, estimated_execution,
	                                       &return_code, &worker_id_v);
	assert(work_admitted == 0 || worker_id_v >= 0);
	assert(work_admitted == 0 || return_code == 1 || return_code == 2);
#endif

	if (work_admitted == 0) {
		assert(worker_id_v < 0);
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 429);
		on_client_response_header_sending(session);
		sandbox_perf_log_print_denied_entry(tenant, route, return_code);
		return;
	}

	/* Allocate a Sandbox */
	session->state          = HTTP_SESSION_EXECUTING;
	struct sandbox *sandbox = sandbox_alloc(route->module, session, work_admitted, now);
	if (unlikely(sandbox == NULL)) {
		// TODO: REDUCE DEMAND!!!
		debuglog("Failed to allocate sandbox\n");
		session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		http_session_set_response_header(session, 500);
		on_client_response_header_sending(session);
		sandbox_perf_log_print_denied_entry(tenant, route, 5000);
		return;
	}

	sandbox->remaining_exec = estimated_execution;
	
#if defined TRAFFIC_CONTROL	
	sandbox->global_queue_type = return_code;
	sandbox->sandbox_meta = global_sandbox_meta;

	if(extra_execution_slack_p > 0) {
		const uint64_t hack = estimated_execution*extra_execution_slack_p/100;
		dbf_list_force_add_extra_slack(global_virt_worker_dbfs[worker_id_v], global_sandbox_meta, hack);
		global_sandbox_meta->extra_slack = hack;
	}

	global_sandbox_meta->sandbox_shadow       = sandbox;
	global_sandbox_meta->id                   = sandbox->id;
	global_sandbox_meta->state                = sandbox->state;

	global_sandbox_meta->worker_id_virt = worker_id_v;
	global_sandbox_meta->global_queue_type = return_code;

	if (global_sandbox_meta->global_queue_type == 2) {
		if(unlikely(priority_queue_enqueue_nolock(tenant->global_sandbox_metas, global_sandbox_meta))){
			panic("Failed to add sandbox_meta to tenant metadata queue");
		}
		global_sandbox_meta->tenant_queue = tenant->global_sandbox_metas;
	}
#endif

	/* If the global request scheduler is full, return a 429 to the client */
	if (unlikely(global_request_scheduler_add(sandbox) == NULL)) {
		// debuglog("Failed to add a %s sandbox to global queue\n", sandbox->tenant->name);
		/////////////////////////////////// TODO ???
		sandbox->response_code = 4290;
		// sandbox->state         = SANDBOX_ERROR;
		// sandbox_perf_log_print_entry(sandbox);
		// sandbox->http = NULL;

		sandbox->timestamp_of.dispatched = now;
		// TODO: REDUCE DEMAND!!!
		sandbox_set_as_error(sandbox, SANDBOX_INITIALIZED);
		free(sandbox->sandbox_meta);
		sandbox_free(sandbox);
		return;
		// session->state = HTTP_SESSION_EXECUTION_COMPLETE;
		// http_session_set_response_header(session, 429);
		// on_client_response_header_sending(session);
		// sandbox_perf_log_print_denied_entry(tenant, route, 999);
	}

	global_sandbox_meta = NULL;
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
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and
 * pushes the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * listener_thread_epoll_file_descriptor - the epoll file descriptor
 *
 */
noreturn void *
listener_thread_main(void *dummy)
{
	struct epoll_event epoll_events[RUNTIME_MAX_EPOLL_EVENTS];

	metrics_server_init();
	listener_thread_register_metrics_server();

	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 2); // TODO: what to do with this?
	pthread_setschedprio(pthread_self(), -20);

#ifdef TRAFFIC_CONTROL
	const int epoll_timeout = 0;
#else
	const int epoll_timeout = -1;
#endif

	while (true) {
#ifdef TRAFFIC_CONTROL
		tenant_database_replenish_all();
		check_messages_from_workers();
#endif		
		/* If -1, Block indefinitely on the epoll file descriptor, waiting on up to a max number of events */
		int descriptor_count = epoll_wait(listener_thread_epoll_file_descriptor, epoll_events,
		                                  RUNTIME_MAX_EPOLL_EVENTS, epoll_timeout);

		if (descriptor_count == 0) continue;
		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic("epoll_wait: %s", strerror(errno));
		}
		assert(descriptor_count > 0);

		for (int i = 0; i < descriptor_count; i++) {
			panic_on_epoll_error(&epoll_events[i]);

			enum epoll_tag tag = *(enum epoll_tag *)epoll_events[i].data.ptr;

			switch (tag) {
			case EPOLL_TAG_TENANT_SERVER_SOCKET:
				// tenant_database_replenish_all();
				// check_messages_from_workers();
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

	panic("Listener thread unexpectedly broke loop\n");
}
