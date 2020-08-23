#pragma once

#include "debuglog.h"
#include "module.h"

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
	assert(module_database_free_offset >= 0);
	assert(module_database_free_offset <= MODULE_MAX_MODULE_COUNT);

	int rc;

	if (module_database_free_offset == MODULE_MAX_MODULE_COUNT) goto err_no_space;
	module_database[module_database_free_offset++] = module;

	rc = 0;
done:
	return rc;
err_no_space:
	debuglog("Cannot add module. Database is full.\n");
	rc = -1;
	goto done;
}
