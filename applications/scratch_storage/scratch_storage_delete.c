#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int scratch_storage_delete(void *key, uint32_t key_len)
  __attribute__((__import_module__("scratch_storage"), __import_name__("delete")));

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

	int rc = scratch_storage_delete(key, strlen(key));
	if (rc == 1) {
		printf("Key '%s' not found\n", key);
		return 0;
	} else {
		printf("Key %s deleted\n", key);
	}
};
