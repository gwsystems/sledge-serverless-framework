#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int scratch_storage_set(void *key, uint32_t key_len, void *value, uint32_t value_len)
  __attribute__((__import_module__("scratch_storage"), __import_name__("set")));

int
main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "%s <key> <value>", argv[0]);
		return 0;
	}

	char *key   = argv[1];
	char *value = argv[2];

	if (key == NULL || strlen(key) < 0 || value == NULL || strlen(value) < 0) {
		fprintf(stderr, "%s <key> <value>", argv[0]);
		return 0;
	}

	int rc = scratch_storage_set(key, strlen(key), value, strlen(value));
	if (rc == 1) {
		printf("Key %s was already present\n", key);
		return 0;
	}

	assert(rc == 0);
	printf("Key %s set to value %s\n", key, value);
	return rc;
};
