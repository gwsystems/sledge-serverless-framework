#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int scratch_storage_upsert(void *key, uint32_t key_len, void *value, uint32_t value_len)
  __attribute__((__import_module__("scratch_storage"), __import_name__("upsert")));

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

	scratch_storage_upsert(key, strlen(key), value, strlen(value));
	printf("Key %s set to value %s\n", key, value);
};
