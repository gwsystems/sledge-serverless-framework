#ifndef SFRT_MODULE_DATABASE_H
#define SFRT_MODULE_DATABASE_H

#include <module.h>

struct module *module_database__find_by_name(char *name);
struct module *module_database__find_by_socket_descriptor(int socket_descriptor);

extern struct module *module_database[];
extern int            module_database__free_offset;

/**
 * Adds a module to the in-memory module DB
 * Note: This was static inline, which I've unwound. I am unclear of the perf implications of this
 * @param module module to add
 * @return 0 on success. Aborts program on failure
 **/
static inline int
module_database__add(struct module *module)
{
	assert(module->socket_descriptor == -1);

	// __sync_fetch_and_add is provided by GCC
	int f = __sync_fetch_and_add(&module_database__free_offset, 1);
	assert(f < MODULE__MAX_MODULE_COUNT);
	module_database[f] = module;

	return 0;
}

#endif /* SFRT_MODULE_DATABASE_H */
