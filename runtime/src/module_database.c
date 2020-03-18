#include <module_database.h>


/***************************************
 * Module Database
 ***************************************/

// In-memory representation of all active modules
struct module *module_database[MODULE__MAX_MODULE_COUNT] = { NULL };
// First free in module
int module_database_free_offset = 0;

/**
 * Given a name, find the associated module
 * @param name
 * @return module or NULL if no match found
 **/
struct module *
module_database__find_by_name(char *name)
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
 **/
struct module *
module_database__find_by_socket_descriptor(int socket_descriptor)
{
	int f = module_database_free_offset;
	for (int i = 0; i < f; i++) {
		assert(module_database[i]);
		if (module_database[i]->socket_descriptor == socket_descriptor) return module_database[i];
	}
	return NULL;
}
