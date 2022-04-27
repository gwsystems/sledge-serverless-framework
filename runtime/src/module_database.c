#include <errno.h>

#include "module_database.h"
#include "panic.h"

/*******************
 * Module Database *
 ******************/

struct module *module_database[MODULE_DATABASE_CAPACITY] = { NULL };
size_t         module_database_count                     = 0;

/**
 * Adds a module to the in-memory module DB
 * @param module module to add
 * @return 0 on success. -ENOSPC when full
 */
int
module_database_add(struct module *module)
{
	assert(module_database_count <= MODULE_DATABASE_CAPACITY);

	int rc;

	if (module_database_count == MODULE_DATABASE_CAPACITY) goto err_no_space;
	module_database[module_database_count++] = module;

	rc = 0;
done:
	return rc;
err_no_space:
	panic("Cannot add module. Database is full.\n");
	rc = -ENOSPC;
	goto done;
}


/**
 * Given a name, find the associated module
 * @param name
 * @return module or NULL if no match found
 */
struct module *
module_database_find_by_name(char *name)
{
	for (size_t i = 0; i < module_database_count; i++) {
		assert(module_database[i]);
		if (strcmp(module_database[i]->name, name) == 0) return module_database[i];
	}
	return NULL;
}

/**
 * Given a socket_descriptor, find the associated module
 * @param socket_descriptor
 * @return module or NULL if no match found
 */
struct module *
module_database_find_by_socket_descriptor(int socket_descriptor)
{
	for (size_t i = 0; i < module_database_count; i++) {
		assert(module_database[i]);
		if (module_database[i]->socket_descriptor == socket_descriptor) return module_database[i];
	}
	return NULL;
}

/**
 * Given a port, find the associated module
 * @param port
 * @return module or NULL if no match found
 */
struct module *
module_database_find_by_port(uint16_t port)
{
	for (size_t i = 0; i < module_database_count; i++) {
		assert(module_database[i]);
		if (module_database[i]->port == port) return module_database[i];
	}
	return NULL;
}
