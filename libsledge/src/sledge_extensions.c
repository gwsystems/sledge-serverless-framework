#include <stdint.h>
#include "sledge_abi.h"

#define INLINE __attribute__((always_inline))

/**
 * @param key
 * @param key_len
 * @returns value_size at key or 0 if key not present
 */
INLINE uint32_t
scratch_storage_get_size(uint32_t key_offset, uint32_t key_len)
{
	return sledge_abi__scratch_storage_get_size(key_offset, key_len);
}

/**
 * @param key_offset
 * @param key_len
 * @param buf_offset linear memory offset to buffer where value should be copied.
 * @param buf_len Size of buffer. Assumed to be size returned by sledge_kv_get_value_size.
 * @returns 0 on success, 1 if key missing, 2 if buffer too small
 */
INLINE int
scratch_storage_get(uint32_t key_offset, uint32_t key_len, uint32_t buf_offset, uint32_t buf_len)
{
	return sledge_abi__scratch_storage_get(key_offset, key_len, buf_offset, buf_len);
};

/**
 * @param key_offset
 * @param key_len
 * @param value_offset
 * @param value_len
 * @returns 0 on success, 1 if already present,
 */
INLINE int
scratch_storage_set(uint32_t key_offset, uint32_t key_len, uint32_t value_offset, uint32_t value_len)
{
	return sledge_abi__scratch_storage_set(key_offset, key_len, value_offset, value_len);
}

/**
 * @param key_offset
 * @param key_len
 * @returns 0 on success, 1 if not present
 */
INLINE int
scratch_storage_delete(uint32_t key_offset, uint32_t key_len)
{
	return sledge_abi__scratch_storage_delete(key_offset, key_len);
}

/**
 * @param key_offset
 * @param key_len
 * @param value_offset
 * @param value_len
 * @returns 0 on success, 1 if already present,
 */
INLINE void
scratch_storage_upsert(uint32_t key_offset, uint32_t key_len, uint32_t value_offset, uint32_t value_len)
{
	sledge_abi__scratch_storage_upsert(key_offset, key_len, value_offset, value_len);
}

/*
 * Return CPU cycles
 */
INLINE uint64_t
env_getcycles() {
        return sledge_abi__env_getcycles();
}

