#pragma once

#include "module.h"

#define MODULE_DATABASE_CAPACITY 1024 

struct module_database {
	struct module *modules[MODULE_DATABASE_CAPACITY];
	size_t         count;
};

struct module *module_database_find_by_path(struct module_database *db, char *path);
void           module_database_init(struct module_database *db);
int            module_database_add(struct module_database *db, struct module *module);
