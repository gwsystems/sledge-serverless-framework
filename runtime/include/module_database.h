#pragma once

#include "module.h"

int            module_database_add(struct module *module);
struct module *module_database_find_by_name(char *name);
struct module *module_database_find_by_socket_descriptor(int socket_descriptor);
