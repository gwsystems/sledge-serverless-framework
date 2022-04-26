#pragma once

#include "http_router.h"
#include "module.h"
#include "tcp_server.h"

#define TENANT_DATABASE_CAPACITY 128

struct tenant {
	char	          *name;
	struct tcp_server      tcp_server;
	struct http_router     router;
	struct module_database module_db;
};
