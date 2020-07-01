#ifndef SFRT_MODULE_DATABASE_H
#define SFRT_MODULE_DATABASE_H

#include <module.h>

struct module *module_database_find_by_name(char *name);
struct module *module_database_find_by_socket_descriptor(int socket_descriptor);

extern struct module *module_database[];
extern int            module_database_free_offset;

/**
 * Adds a module to the in-memory module DB
 * @param module module to add
 * @return 0 on success. Aborts program on failure
 */
static inline int
module_database_add(struct module *module)
{
	assert(module->socket_descriptor == -1);

	int f = __sync_fetch_and_add(&module_database_free_offset, 1);
	assert(f < MODULE_MAX_MODULE_COUNT);
	module_database[f] = module;

	return 0;
}

#endif /* SFRT_MODULE_DATABASE_H */
