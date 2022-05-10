#pragma once

#include "http_router.h"
#include "module_database.h"
#include "tcp_server.h"

struct tenant {
	char                  *name;
	struct tcp_server      tcp_server;
	http_router_t          router;
	struct module_database module_db;
};
