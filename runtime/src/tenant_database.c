#include <errno.h>

#include "runtime.h"
#include "tenant.h"
#include "panic.h"

/*******************
 * Tenant Database *
 ******************/

struct tenant *tenant_database[RUNTIME_MAX_TENANT_COUNT] = { NULL };
size_t         tenant_database_count                     = 0;

/**
 * Adds a tenant to the in-memory tenant DB
 * @param tenant tenant to add
 * @return 0 on success. -ENOSPC when full
 */
int
tenant_database_add(struct tenant *tenant)
{
	assert(tenant_database_count <= RUNTIME_MAX_TENANT_COUNT);

	int rc;

	if (tenant_database_count == RUNTIME_MAX_TENANT_COUNT) goto err_no_space;
	tenant_database[tenant_database_count++] = tenant;

	rc = 0;
done:
	return rc;
err_no_space:
	panic("Cannot add tenant. Database is full.\n");
	rc = -ENOSPC;
	goto done;
}


/**
 * Given a name, find the associated tenant
 * @param name
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_name(char *name)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (strcmp(tenant_database[i]->name, name) == 0) return tenant_database[i];
	}
	return NULL;
}

/**
 * Given a socket_descriptor, find the associated tenant
 * @param socket_descriptor
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_socket_descriptor(int socket_descriptor)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i]->tcp_server.socket_descriptor == socket_descriptor) return tenant_database[i];
	}
	return NULL;
}

/**
 * Given a port, find the associated tenant
 * @param port
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_port(uint16_t port)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i]->tcp_server.port == port) return tenant_database[i];
	}
	return NULL;
}

/**
 * Checks is an opaque pointer is a tenant by comparing against
 */
struct tenant *
tenant_database_find_by_ptr(void *ptr)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i] == ptr) return tenant_database[i];
	}
	return NULL;
}
