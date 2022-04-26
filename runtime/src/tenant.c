#include "tenant.h"
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

	/* Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP requests
	 */

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
