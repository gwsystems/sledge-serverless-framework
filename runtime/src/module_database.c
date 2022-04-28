#include <errno.h>

#include "module_database.h"
#include "panic.h"

/*******************
 * Module Database *
 ******************/

void
module_database_init(struct module_database *db)
{
	db->count = 0;
}

/**
 * Adds a module to the in-memory module DB
 * @param module module to add
 * @return 0 on success. -ENOSPC when full
 */
static int
module_database_add(struct module_database *db, struct module *module)
{
	assert(db->count <= MODULE_DATABASE_CAPACITY);

	int rc;

	if (db->count == MODULE_DATABASE_CAPACITY) goto err_no_space;
	db->modules[db->count++] = module;

	rc = 0;
done:
	return rc;
err_no_space:
	panic("Cannot add module. Database is full.\n");
	rc = -ENOSPC;
	goto done;
}


/**
 * Given a path, find the associated module
 * @param name
 * @return module or NULL if no match found
 */
struct module *
module_database_find_by_path(struct module_database *db, char *path)
{
	for (size_t i = 0; i < db->count; i++) {
		assert(db->modules[i]);
		if (strcmp(db->modules[i]->path, path) == 0) return db->modules[i];
	}

	struct module *module = module_alloc(path);
	if (module != NULL) {
		module_database_add(db, module);
		return module;
	}

	return NULL;
}
