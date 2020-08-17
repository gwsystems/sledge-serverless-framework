#pragma once

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
	/*
	 * Assumption: Module is added to database before being listened to
	 * TODO: Why does this matter?
	 */
	assert(module->socket_descriptor == -1);

	int rc;

	if (module_database_free_offset >= MODULE_MAX_MODULE_COUNT) goto err_no_space;

	int f = __sync_fetch_and_add(&module_database_free_offset, 1);
	if (module_database_free_offset > MODULE_MAX_MODULE_COUNT) {
		__sync_fetch_and_subtract(&module_database_free_offset, 1);
		goto err_no_space;
	}

	if (module_database_free_offset == MODULE_MAX_MODULE_COUNT) goto err_no_space;
	assert(f < MODULE_MAX_MODULE_COUNT);
	module_database[f] = module;

	rc = 0;
done:
	return rc;
err_no_space:
	debuglog("Cannot add module. Database is full.\n");
	rc = -1;
	goto done;
}
