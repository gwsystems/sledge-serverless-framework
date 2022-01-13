#pragma once

#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>

#include "debuglog.h"
#include "wasm_types.h"
#include "sledge_abi.h"

struct sledge_abi_symbols {
	void *                        handle;
	sledge_abi__init_globals_fn_t initialize_globals;
	sledge_abi__init_mem_fn_t     initialize_memory;
	sledge_abi__init_tbl_fn_t     initialize_tables;
	sledge_abi__entrypoint_fn_t   entrypoint;
	uint32_t                      starting_pages;
	uint32_t                      max_pages;
};

/* Initializes the ABI object using the *.so file at path */
static inline int
sledge_abi_symbols_init(struct sledge_abi_symbols *abi, char *path)
{
	assert(abi != NULL);

	int rc = 0;

	abi->handle = dlopen(path, RTLD_LAZY | RTLD_DEEPBIND);
	if (abi->handle == NULL) {
		fprintf(stderr, "Failed to open %s with error: %s\n", path, dlerror());
		goto dl_open_error;
	};

	/* Resolve the symbols in the dynamic library *.so file */
	abi->entrypoint = (sledge_abi__entrypoint_fn_t)dlsym(abi->handle, SLEDGE_ABI__ENTRYPOINT);
	if (abi->entrypoint == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", SLEDGE_ABI__ENTRYPOINT, path,
		        dlerror());
		goto dl_error;
	}

	/*
	 * This symbol may or may not be present depending on whether the aWsm was
	 * run with the --runtime-globals flag. It is not clear what the proper
	 * configuration would be for SLEdge, so no validation is performed
	 */
	abi->initialize_globals = (sledge_abi__init_globals_fn_t)dlsym(abi->handle, SLEDGE_ABI__INITIALIZE_GLOBALS);

	abi->initialize_memory = (sledge_abi__init_mem_fn_t)dlsym(abi->handle, SLEDGE_ABI__INITIALIZE_MEMORY);
	if (abi->initialize_memory == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", SLEDGE_ABI__INITIALIZE_MEMORY,
		        path, dlerror());
		goto dl_error;
	};

	abi->initialize_tables = (sledge_abi__init_tbl_fn_t)dlsym(abi->handle, SLEDGE_ABI__INITIALIZE_TABLE);
	if (abi->initialize_tables == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", SLEDGE_ABI__INITIALIZE_TABLE,
		        path, dlerror());
		goto dl_error;
	};


	abi->starting_pages = *(uint32_t *)dlsym(abi->handle, SLEDGE_ABI__STARTING_PAGES);
	if (abi->starting_pages == 0) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", SLEDGE_ABI__STARTING_PAGES, path,
		        dlerror());
		goto dl_error;
	}

	abi->max_pages = *(uint32_t *)dlsym(abi->handle, SLEDGE_ABI__MAX_PAGES);
	if (abi->max_pages == 0) {
		/* This seems to not always be present. I assume this is only there if the source module explicitly
		 * specified this */
		abi->max_pages = WASM_MEMORY_PAGES_MAX;
		debuglog("max_pages symbols not defined. Defaulting to MAX defined by spec.\n");

		// TODO: We need to prove that this actually can get generated by awsm
		// fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", SLEDGE_ABI__MAX_PAGES, path,
		//         dlerror());
		//   goto dl_error;
	}


done:
	return rc;
dl_error:
	dlclose(abi->handle);
dl_open_error:
	rc = -1;
	goto done;
}

static inline int
sledge_abi_symbols_deinit(struct sledge_abi_symbols *abi)
{
	abi->entrypoint         = NULL;
	abi->initialize_globals = NULL;
	abi->initialize_memory  = NULL;
	abi->initialize_tables  = NULL;

	int rc = dlclose(abi->handle);
	if (rc != 0) {
		fprintf(stderr, "Failed to close *.so file with error: %s\n", dlerror());
		return 1;
	}

	return 0;
}