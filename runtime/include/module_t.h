#pragma once

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "admissions_info.h"
#include "types.h"

#define MODULE_MAX_NAME_LENGTH 32
#define MODULE_MAX_PATH_LENGTH 256

struct module {
	char                        name[MODULE_MAX_NAME_LENGTH];
	char                        path[MODULE_MAX_PATH_LENGTH];
	void *                      dynamic_library_handle; /* Handle to the *.so of the serverless function */
	int32_t                     argument_count;
	uint32_t                    stack_size; /* a specification? */
	uint64_t                    max_memory; /* perhaps a specification of the module. (max 4GB) */
	uint32_t                    relative_deadline_us;
	uint64_t                    relative_deadline; /* cycles */
	_Atomic uint32_t            reference_count;   /* ref count how many instances exist here. */
	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];
	struct sockaddr_in          socket_address;
	int                         socket_descriptor;
	struct admissions_info      admissions_info;
	int                         port;

	/*
	 * unfortunately, using UV for accepting connections is not great!
	 * on_connection, to create a new accepted connection, will have to init a tcp handle,
	 * which requires a uvloop. cannot use main as rest of the connection is handled in
	 * sandboxing threads, with per-core(per-thread) tls data-structures.
	 * so, using direct epoll for accepting connections.
	 */

	unsigned long max_request_size;
	char          request_headers[HTTP_MAX_HEADER_COUNT][HTTP_MAX_HEADER_LENGTH];
	int           request_header_count;
	char          request_content_type[HTTP_MAX_HEADER_VALUE_LENGTH];

	/* resp size including headers! */
	unsigned long max_response_size;
	int           response_header_count;
	char          response_content_type[HTTP_MAX_HEADER_VALUE_LENGTH];
	char          response_headers[HTTP_MAX_HEADER_COUNT][HTTP_MAX_HEADER_LENGTH];

	/* Equals the largest of either max_request_size or max_response_size */
	unsigned long max_request_or_response_size;

	/* Functions to initialize aspects of sandbox */
	mod_glb_fn_t  initialize_globals;
	mod_mem_fn_t  initialize_memory;
	mod_tbl_fn_t  initialize_tables;
	mod_libc_fn_t initialize_libc;

	/* Entry Function to invoke serverless function */
	mod_main_fn_t main;
};
