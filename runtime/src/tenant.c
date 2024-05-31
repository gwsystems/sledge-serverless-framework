#include "tenant.h"
#include "current_sandbox.h"
#include "tenant_functions.h"

/**
 * Start the tenant as a server listening at tenant->port
 * @param tenant
 * @returns 0 on success, -1 on error
 */
int
tenant_listen(struct tenant *tenant)
{
	int rc = tcp_server_listen(&tenant->tcp_server);
	if (rc < 0) goto err;

	rc = listener_thread_register_tenant(tenant);
	if (unlikely(rc < 0)) goto err_add_to_epoll;

done:
	return rc;
err_add_to_epoll:
	tcp_server_close(&tenant->tcp_server);
err:
	rc = -1;
	goto done;
}

#ifdef EXECUTION_REGRESSION
void
tenant_preprocess(struct http_session *session)
{
	/* No tenant preprocessing if the wasm module not provided by the tenant */
	if (session->route->module_proprocess == NULL) goto done;
	const uint64_t start = __getcycles();

	/* Tenant Pre-processing - Extract other useful parameters */
	struct sandbox *pre_sandbox = sandbox_alloc(session->route->module_proprocess, session, session->route,
	                                            session->tenant, 1);
	if (sandbox_prepare_execution_environment(pre_sandbox)) panic("pre_sandbox environment setup failed");
	pre_sandbox->state = SANDBOX_RUNNING_SYS;

	assert(current_sandbox_get() == NULL);
	current_sandbox_set(pre_sandbox);
	current_sandbox_start();
	auto_buf_flush(&session->response_body);

	char *endptr;
	long  num = strtol(session->response_body.data, &endptr, 10);
	if (endptr == session->response_body.data) {
		printf("No digits were found\n");
	} else if (*endptr != '\0' && *endptr != '\n') {
		printf("Further characters after number: %s\n", endptr);
	} else if ((errno == ERANGE && (num == LONG_MAX || num == LONG_MIN)) || (errno != 0 && num == 0)) {
		perror("strtol");
	} else {
		session->regression_param = num;
	}

	session->http_request.cursor = 0;
	pre_sandbox->http            = NULL;
	pre_sandbox->state           = SANDBOX_COMPLETE;
	auto_buf_deinit(&session->response_body);
	current_sandbox_set(NULL);
	sandbox_free_linear_memory(pre_sandbox);
	sandbox_free(pre_sandbox);

	const uint64_t end              = __getcycles();
	session->preprocessing_duration = end - start;

done:
	session->did_preprocessing = true;
}
#endif
