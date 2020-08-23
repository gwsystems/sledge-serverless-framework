#include "module_database.h"

/*******************
 * Module Database *
 ******************/

struct module *module_database[MODULE_MAX_MODULE_COUNT] = { NULL };
size_t         module_database_count                    = 0;

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
