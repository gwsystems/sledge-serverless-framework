#include "module_database.h"

/*******************
 * Module Database *
 ******************/

struct module *module_database[MODULE_MAX_MODULE_COUNT] = { NULL }; /* In-memory representation of all active modules */
int            module_database_free_offset              = 0;        /* First free in module */

/**
 * Given a name, find the associated module
 * @param name
 * @return module or NULL if no match found
 */
struct module *
module_database_find_by_name(char *name)
{
	int f = module_database_free_offset;
	for (int i = 0; i < f; i++) {
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
	int f = module_database_free_offset;
	for (int i = 0; i < f; i++) {
		assert(module_database[i]);
		if (module_database[i]->socket_descriptor == socket_descriptor) return module_database[i];
	}
	return NULL;
}
