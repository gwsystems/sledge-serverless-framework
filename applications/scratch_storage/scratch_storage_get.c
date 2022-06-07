#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int scratch_storage_get(void *key, uint32_t key_len, void *buf, uint32_t buf_len)
  __attribute__((__import_module__("scratch_storage"), __import_name__("get")));

extern uint32_t scratch_storage_get_size(void *key, uint32_t key_len)
  __attribute__((__import_module__("scratch_storage"), __import_name__("get_size")));

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "%s <key>", argv[0]);
		return 0;
	}

	char *key = argv[1];

	if (key == NULL || strlen(key) < 0) {
		fprintf(stderr, "%s <key>", argv[0]);
		return 0;
	}

	uint32_t val_size = scratch_storage_get_size(key, strlen(key));
	char    *buf      = calloc(val_size + 1, sizeof(char));
	int      rc       = scratch_storage_get(key, strlen(key), buf, val_size);
	assert(rc != 2);
	if (rc == 1) {
		printf("Key '%s' not found\n", key);
		return 0;
	} else {
		printf("Key %s resolved to value of size %u with contents %s\n", key, val_size, buf);
	}
};
