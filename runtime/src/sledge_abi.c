#include "sledge_abi.h"
#include "current_sandbox.h"
#include "map.h"
#include "sandbox_set_as_running_sys.h"
#include "sandbox_set_as_running_user.h"
#include "wasi.h"
#include "wasi_serdes.h"
#include "wasm_memory.h"

EXPORT void
sledge_abi__wasm_trap_raise(enum sledge_abi__wasm_trap trapno)
{
	return current_sandbox_trap(trapno);
}

/**
 * @brief Get the memory ptr for runtime object
 *
 * @param offset base offset of pointer
 * @param length length starting at base offset
 * @return host address of offset into WebAssembly linear memory
 */
static inline char *
get_memory_ptr_for_runtime(uint32_t offset, uint32_t length)
{
	assert((uint64_t)offset + length < sledge_abi__current_wasm_module_instance.abi.memory.size);
	char *mem_as_chars = (char *)sledge_abi__current_wasm_module_instance.abi.memory.buffer;
	char *address      = &mem_as_chars[offset];
	return address;
}

static inline void
check_bounds(uint32_t offset, uint32_t bounds_check)
{
	// Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds <
	// WASM_PAGE_SIZE
	assert(bounds_check < WASM_PAGE_SIZE
	       || (sledge_abi__current_wasm_module_instance.abi.memory.size > bounds_check
	           && offset <= sledge_abi__current_wasm_module_instance.abi.memory.size - bounds_check));
}

// TODO: Don't need to pass the memory here
EXPORT int32_t
sledge_abi__wasm_memory_expand(struct sledge_abi__wasm_memory *wasm_memory, uint32_t page_count)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	int32_t old_page_count = wasm_memory->size / WASM_PAGE_SIZE;
	int     rc             = wasm_memory_expand((struct wasm_memory *)wasm_memory, page_count * WASM_PAGE_SIZE);

	if (unlikely(rc == -1)) {
		old_page_count = -1;
		goto DONE;
	}

	/* We updated "forked state" in sledge_abi__current_wasm_module_instance.memory. We need to write this back to
	 * the original struct as well  */
	current_sandbox_memory_writeback();

#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// Cache the runtime of the first N page allocations
	for (int i = 0; i < page_count; i++) {
		if (likely(sandbox->timestamp_of.page_allocations_size < SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT)) {
			sandbox->timestamp_of.page_allocations[sandbox->timestamp_of.page_allocations_size++] =
			  sandbox->duration_of_state.running
			  + (uint32_t)(__getcycles() - sandbox->timestamp_of.last_state_change);
		}
	}
#endif

DONE:
	sandbox_return(sandbox);

	return old_page_count;
}

// TODO: Don't need to pass the memory here
EXPORT void
sledge_abi__wasm_memory_initialize_region(struct sledge_abi__wasm_memory *wasm_memory, uint32_t offset,
                                          uint32_t region_size, uint8_t region[])
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	wasm_memory_initialize_region((struct wasm_memory *)wasm_memory, offset, region_size, region);
}

EXPORT int32_t
sledge_abi__wasm_globals_get_i32(uint32_t idx)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	int32_t result;
	int     rc = wasm_globals_get_i32(&sandbox->globals, idx, &result);
	sandbox_return(sandbox);

	if (rc == -1) sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX);
	if (rc == -2) sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE);

	return result;
}

EXPORT int64_t
sledge_abi__wasm_globals_get_i64(uint32_t idx)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	int64_t result;
	int     rc = wasm_globals_get_i64(&sandbox->globals, idx, &result);
	sandbox_return(sandbox);

	if (rc == -1) sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX);
	if (rc == -2) sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE);

	return result;
}

EXPORT void
sledge_abi__wasm_globals_set_i32(uint32_t idx, int32_t value, bool is_mutable)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	int32_t rc = wasm_globals_set_i32(&sandbox->globals, idx, value, true);
	sandbox_return(sandbox);

	if (rc == -1) sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX);
	if (rc == -2) sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE);
}

// 0 on success, -1 on out of bounds, -2 on mismatched type
EXPORT void
sledge_abi__wasm_globals_set_i64(uint32_t idx, int64_t value, bool is_mutable)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	int32_t rc = wasm_globals_set_i64(&sandbox->globals, idx, value, true);
	sandbox_return(sandbox);

	if (rc == -1) sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX);
	if (rc == -2) sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE);
}

/**
 * @brief Writes argument offsets and buffer into linear memory
 *
 * @param argv_retoffset
 * @param argv_buf_retoffset
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_args_get(__wasi_size_t argv_retoffset, __wasi_size_t argv_buf_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	__wasi_size_t       rc   = 0;
	const __wasi_size_t argc = sandbox->wasi_context->argc;
	if (unlikely(argc == 0)) { goto done; }

	__wasi_size_t      *argv_retptr     = (__wasi_size_t *)get_memory_ptr_for_runtime(argv_retoffset,
	                                                                                  WASI_SERDES_SIZE_size_t * argc);
	const __wasi_size_t argv_buf_size   = sandbox->wasi_context->argv_buf_size;
	char               *argv_buf_retptr = get_memory_ptr_for_runtime(argv_buf_retoffset, argv_buf_size);

	/* args_get backings return a vector of host pointers. We need a host buffer to store this
	 * temporarily before unswizzling and writing to linear memory */
	char **argv_temp = calloc(argc, sizeof(char *));
	if (unlikely(argv_temp == NULL)) { goto done; }

	/* Writes argv_buf to linear memory and argv vector to our temporary buffer */
	rc = wasi_snapshot_preview1_backing_args_get(sandbox->wasi_context, argv_temp, argv_buf_retptr);


	if (unlikely(rc != __WASI_ERRNO_SUCCESS)) { goto done; }

	/* Unswizzle argv */
	for (int i = 0; i < argc; i++) {
		argv_retptr[i] = argv_buf_retoffset + (uint32_t)(argv_temp[i] - argv_temp[0]);
	}

done:
	if (likely(argv_temp != NULL)) {
		free(argv_temp);
		argv_temp = NULL;
	}
	sandbox_return(sandbox);

	return (uint32_t)rc;
}

/**
 * @brief Used by a WASI module to determine the argument count and size of the requried
 * argument buffer
 *
 * @param argc_retoffset linear memory offset where we should write argc
 * @param argv_buf_len_retoffset linear memory offset where we should write the length of the args buffer
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_args_sizes_get(__wasi_size_t argc_retoffset, __wasi_size_t argv_buf_len_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_size_t *argc_retptr         = (__wasi_size_t *)get_memory_ptr_for_runtime(argc_retoffset,
	                                                                                 WASI_SERDES_SIZE_size_t);
	__wasi_size_t *argv_buf_len_retptr = (__wasi_size_t *)get_memory_ptr_for_runtime(argv_buf_len_retoffset,
	                                                                                 WASI_SERDES_SIZE_size_t);

	uint32_t rc = wasi_snapshot_preview1_backing_args_sizes_get(sandbox->wasi_context, argc_retptr,
	                                                            argv_buf_len_retptr);
	sandbox_return(sandbox);

	return rc;
}

/**
 * @brief Return the resolution of a clock
 * Implementations are required to provide a non-zero value for supported clocks. For unsupported clocks,
 * return `errno::inval`.
 *
 * @param id The clock for which to return the resolution.
 * @param resolution_retoffset - The resolution of the clock
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_clock_res_get(__wasi_clockid_t id, __wasi_size_t resolution_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Return the time value of a clock
 *
 * @param clock_id The clock for which to return the time.
 * @param precision The maximum lag (exclusive) that the returned time value may have, compared to its actual value.
 * @param time_retoffset  The time value of the clock.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_clock_time_get(__wasi_clockid_t clock_id, __wasi_timestamp_t precision,
                                                  __wasi_size_t time_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_timestamp_t *time_retptr = (__wasi_timestamp_t *)
	  get_memory_ptr_for_runtime(time_retoffset, WASI_SERDES_SIZE_timestamp_t);

	uint32_t rc = wasi_snapshot_preview1_backing_clock_time_get(sandbox->wasi_context, clock_id, precision,
	                                                            time_retptr);
	sandbox_return(sandbox);

	return rc;
}

/**
 * Read environment variable data.
 * The sizes of the buffers should match that returned by `environ_sizes_get`.
 *
 * @param environ_retoffset
 * @param environ_buf_retoffset
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_environ_get(__wasi_size_t env_retoffset, __wasi_size_t env_buf_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_errno_t rc = 0;

	const __wasi_size_t envc = sandbox->wasi_context->envc;
	if (envc == 0) { goto done; }

	const __wasi_size_t env_buf_size = sandbox->wasi_context->env_buf_size;
	assert(env_buf_size > envc);

	/* wasi_snapshot_preview1_backing_environ_get returns a vector of host pointers. We write
	 * these results to environ_temp temporarily before converting to offsets and writing to
	 * linear memory. We could technically write this to linear memory and then do a "fix up,"
	 * but this would leak host information and constitue a security issue */
	char **env_temp = calloc(envc, sizeof(char *));
	if (unlikely(env_temp == NULL)) { goto done; }

	__wasi_size_t *env_retptr     = (__wasi_size_t *)get_memory_ptr_for_runtime(env_retoffset,
	                                                                            WASI_SERDES_SIZE_size_t * envc);
	char          *env_buf_retptr = get_memory_ptr_for_runtime(env_buf_retoffset, env_buf_size);

	rc = wasi_snapshot_preview1_backing_environ_get(sandbox->wasi_context, env_temp, env_buf_retptr);
	if (unlikely(rc != __WASI_ERRNO_SUCCESS)) { goto done; }

	/* Unswizzle env and write to linear memory */
	for (int i = 0; i < envc; i++) { env_retptr[i] = env_buf_retoffset + (uint32_t)(env_temp[i] - env_temp[0]); }

done:
	if (likely(env_temp != NULL)) {
		free(env_temp);
		env_temp = NULL;
	}

	sandbox_return(sandbox);
	return (uint32_t)rc;
}

/**
 * Returns the number of environment variable arguments and the size of the environment variable data.
 *
 * @param envc_retoffset - the offset where the resulting number of environment variable arguments should be written
 * @param env_buf_len_retoffset - the offset where the resulting size of the environment variable data should be
 * written
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_environ_sizes_get(__wasi_size_t envc_retoffset, __wasi_size_t env_buf_len_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_size_t *envc_retptr        = (__wasi_size_t *)get_memory_ptr_for_runtime(envc_retoffset,
	                                                                                WASI_SERDES_SIZE_size_t);
	__wasi_size_t *env_buf_len_retptr = (__wasi_size_t *)get_memory_ptr_for_runtime(env_buf_len_retoffset,
	                                                                                WASI_SERDES_SIZE_size_t);

	uint32_t rc = wasi_snapshot_preview1_backing_environ_sizes_get(sandbox->wasi_context, envc_retptr,
	                                                               env_buf_len_retptr);

	sandbox_return(sandbox);

	return rc;
}

/**
 * Provide file advisory information on a file descriptor.
 * Note: similar to `posix_fadvise` in POSIX
 *
 * @param fd
 * @param file_offset The offset within the file to which the advisory applies.
 * @param len The length of the region to which the advisory applies.
 * @param advice
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_advise(__wasi_fd_t fd, __wasi_filesize_t file_offset, __wasi_filesize_t len,
                                             uint32_t advice_extended)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Force the allocation of space in a file.
 *
 * @param fd
 * @param offset The offset at which to start the allocation.
 * @param len The length of the area that is allocated.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_allocate(__wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Close a file descriptor.
 *
 * @param fd
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_close(__wasi_fd_t fd)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Synchronize the data of a file to disk.
 *
 * @param fd
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_datasync(__wasi_fd_t fd)
{
	return wasi_unsupported_syscall(__func__);
}


/**
 * Get the attributes of a file descriptor.
 *
 * @param fd
 * @param fdstat_retoffset return param of resulting wasi_fdstat structure
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_fdstat_get(__wasi_fd_t fd, __wasi_size_t fdstat_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Adjust the flags associated with a file descriptor
 *
 * @param fd
 * @param fdflags_extended The desired values of the file descriptor flags, zero extended to 32-bits
 * @return WASI_ESUCCESS, WASI_EACCES, WASI_EAGAIN, WASI_EBADF, WASI_EFAULT, WASI_EINVAL, WASI_ENOENT, or WASI_EPERM
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_fdstat_set_flags(__wasi_fd_t fd, uint32_t fdflags_extended)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Adjust the rights associated with a file descriptor.
 * This can only be used to remove rights, and returns `errno::notcapable` if called in a way that would attempt to add
 * rights
 *
 * @param fd
 * @param fs_rights_base The desired rights of the file descriptor.
 * @param fs_rights_inheriting
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_fdstat_set_rights(__wasi_fd_t fd, __wasi_rights_t fs_rights_base,
                                                        __wasi_rights_t fs_rights_inheriting)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Return the attributes of an open file.
 *
 * @param fd
 * @param filestat_retoffset The buffer where we should store the file's attributes
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_filestat_get(__wasi_fd_t fd, __wasi_size_t filestat_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Adjust the size of an open file, zeroing extra bytes on increase
 *
 * @param fd
 * @param size The desired file size.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_filestat_set_size(__wasi_fd_t fd, __wasi_filesize_t size)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Adjust the timestamps of an open file or directory
 *
 * @param fd
 * @param atim The desired values of the data access timestamp.
 * @param mtim The desired values of the data modification timestamp.
 * @param fstflags A bitmask indicating which timestamps to adjust.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_filestat_set_times(__wasi_fd_t fd, __wasi_timestamp_t atim,
                                                         __wasi_timestamp_t mtim, uint32_t fstflags_extended)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Read from a file descriptor without updating the descriptor's offset
 *
 * @param fd
 * @param iovs_baseoffset List of scatter/gather vectors in which to store data.
 * @param iovs_len The length of the iovs vector
 * @param offset The offset within the file at which to read.
 * @param nread_retoffset The number of bytes read.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_pread(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                            __wasi_filesize_t offset, __wasi_size_t nread_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Return a description of the given preopened file descriptor.
 *
 * @param fd
 * @param prestat_retoffset The buffer where the description is stored.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_prestat_get(__wasi_fd_t fd, __wasi_size_t prestat_retoffset)
{
	/* This signals that there are no file descriptors */
	return __WASI_ERRNO_BADF;
}

/**
 * Return a description of the given preopened file descriptor.
 *
 * @param fd
 * @param dirname_retoffset A buffer into which to write the preopened directory name.
 * @param dirname_len The length of the buffer at path_retptr
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_prestat_dir_name(__wasi_fd_t fd, __wasi_size_t dirname_retoffset,
                                                       __wasi_size_t dirname_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Write to a file descriptor without updating the descriptor's offset
 *
 * @param fd
 * @param iovs_baseoffset List of scatter/gather vectors from which to retrieve data.
 * @param iovs_len The length of the array pointed to by `iovs`.
 * @param offset The offset within the file at which to write.
 * @param nwritten_retoffset The number of bytes written.
 * @return status code
 *
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_pwrite(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                             __wasi_filesize_t file_offset, __wasi_size_t nwritten_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Read from a file descriptor
 *
 * @param fd
 * @param iovs_baseptr
 * @param iovs_len
 * @param nread_retoffset The number of bytes read.
 * @return WASI_ESUCCESS, WASI_EAGAIN, WASI_EWOULDBLOCK, WASI_EBADF, WASI_EFAULT, WASI_EINTR, WASI_EIO, WASI_EISDIR, or
 * others
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_read(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                           __wasi_size_t nread_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_errno_t rc           = 0;
	__wasi_size_t *nread_retptr = (__wasi_size_t *)get_memory_ptr_for_runtime(nread_retoffset,
	                                                                          WASI_SERDES_SIZE_size_t);

	/* Swizzle iovs, writting to temp buffer */
	check_bounds(iovs_baseoffset, WASI_SERDES_SIZE_iovec_t * iovs_len);
	__wasi_iovec_t *iovs_baseptr = calloc(iovs_len, sizeof(__wasi_iovec_t));
	if (unlikely(iovs_baseptr == NULL)) { goto done; }
	rc = wasi_serdes_readv_iovec_t(sandbox->memory->abi.buffer, sandbox->memory->abi.size, iovs_baseoffset,
	                               iovs_baseptr, iovs_len);
	if (unlikely(rc != __WASI_ERRNO_SUCCESS)) { goto done; }

	rc = wasi_snapshot_preview1_backing_fd_read(sandbox->wasi_context, fd, iovs_baseptr, iovs_len, nread_retptr);

done:
	if (likely(iovs_baseptr != NULL)) {
		free(iovs_baseptr);
		iovs_baseptr = NULL;
	}

	sandbox_return(sandbox);
	return (uint32_t)rc;
}

/**
 * Atomically replace a file descriptor by renumbering another file descriptor.
 * Due to the strong focus on thread safety, this environment does not provide
 * a mechanism to duplicate or renumber a file descriptor to an arbitrary
 * number, like `dup2()`. This would be prone to race conditions, as an actual
 * file descriptor with the same number could be allocated by a different
 * thread at the same time.
 * This function provides a way to atomically renumber file descriptors, which
 * would disappear if `dup2()` were to be removed entirely.
 *
 * @param fd
 * @param to the file descriptor to overwrite
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_readdir(__wasi_fd_t fd, __wasi_size_t buf_baseoffset, __wasi_size_t buf_len,
                                              __wasi_dircookie_t cookie, __wasi_size_t nread_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Atomically replace a file descriptor by renumbering another file descriptor.
 * Due to the strong focus on thread safety, this environment does not provide
 * a mechanism to duplicate or renumber a file descriptor to an arbitrary
 * number, like `dup2()`. This would be prone to race conditions, as an actual
 * file descriptor with the same number could be allocated by a different
 * thread at the same time.
 * This function provides a way to atomically renumber file descriptors, which
 * would disappear if `dup2()` were to be removed entirely.
 *
 * @param fd
 * @param to the file descriptor to overwrite
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_renumber(__wasi_fd_t fd, __wasi_fd_t to)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Move the offset of a file descriptor
 *
 * @param fd
 * @param file_offset The number of bytes to move.
 * @param whence_extended The base from which the offset is relative.
 * @param newoffset_retoffset The new offset of the file descriptor, relative to the start of the file.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_seek(__wasi_fd_t fd, __wasi_filedelta_t file_offset, uint32_t whence_extended,
                                           __wasi_size_t newoffset_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Synchronize the data and metadata of a file to disk
 *
 * @param fd
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_sync(__wasi_fd_t fd)
{
	return wasi_unsupported_syscall(__func__);
}


/**
 * Return the current offset of a file descriptor
 *
 * @param fd
 * @param fileoffset_retoffset The current offset of the file descriptor, relative to the start of the file.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_tell(__wasi_fd_t fd, __wasi_size_t fileoffset_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Write to a file descriptor
 *
 * @param fd
 * @param iovs_baseoffset List of scatter/gather vectors from which to retrieve data.
 * @param iovs_len The length of the array pointed to by `iovs`.
 * @param nwritten_retoffset
 * @return WASI_ESUCCESS, WASI_EAGAIN, WASI_EWOULDBLOCK, WASI_EBADF, WASI_EFAULT,
 * WASI_EFBIG, WASI_EINTR, WASI_EIO, WASI_ENOSPC, WASI_EPERM, WASI_EPIPE, or others
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_fd_write(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                            __wasi_size_t nwritten_retoffset)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	__wasi_errno_t rc              = 0;
	__wasi_size_t *nwritten_retptr = (__wasi_size_t *)get_memory_ptr_for_runtime(nwritten_retoffset,
	                                                                             WASI_SERDES_SIZE_size_t);

	/* Swizzle iovs, writting to temporary buffer */
	check_bounds(iovs_baseoffset, WASI_SERDES_SIZE_ciovec_t * iovs_len);
	__wasi_ciovec_t *iovs_baseptr = calloc(iovs_len, sizeof(__wasi_ciovec_t));
	if (unlikely(iovs_baseptr == NULL)) { goto done; }
	rc = wasi_serdes_readv_ciovec_t(sandbox->memory->abi.buffer, sandbox->memory->abi.size, iovs_baseoffset,
	                                iovs_baseptr, iovs_len);
	if (unlikely(rc != __WASI_ERRNO_SUCCESS)) { goto done; }

	rc = wasi_snapshot_preview1_backing_fd_write(sandbox->wasi_context, fd, iovs_baseptr, iovs_len,
	                                             nwritten_retptr);

done:
	sandbox_return(sandbox);
	if (likely(iovs_baseptr != NULL)) {
		free(iovs_baseptr);
		iovs_baseptr = NULL;
	}

	return (uint32_t)rc;
}

/**
 * Create a directory
 *
 * @param fd
 * @param path_baseoffset
 * @param path_len
 * @return WASI_ESUCCESS, WASI_EACCES, WASI_EBADF, WASI_EDQUOT, WASI_EEXIST,
 * WASI_EFAULT, WASI_EINVAL, WASI_ELOOP, WASI_EMLINK, WASI_ENAMETOOLONG,
 * WASI_ENOENT, WASI_ENOMEM, WASI_ENOSPC, WASI_ENOTDIR, WASI_EPERM, or WASI_EROFS
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_create_directory(__wasi_fd_t fd, __wasi_size_t path_baseoffset,
                                                         __wasi_size_t path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Return the attributes of a file or directory
 *
 * @param fd
 * @param flags Flags determining the method of how the path is resolved.
 * @param path_baseoffset The path of the file or directory to inspect.
 * @param filestat_retoffset The buffer where the file's attributes are stored.
 * @return WASI_ESUCCESS, WASI_EACCES, WASI_EBAD, WASI_EFAUL, WASI_EINVAL, WASI_ELOOP,
 * WASI_ENAMETOOLON, WASI_ENOENT, WASI_ENOENT, WASI_ENOMEM, WASI_ENOTDI, or WASI_EOVERFLOW
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_filestat_get(__wasi_fd_t fd, __wasi_lookupflags_t flags,
                                                     __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                                     __wasi_size_t filestat_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Adjust the timestamps of a file or directory
 *
 * @param fd
 * @param flags Flags determining the method of how the path is resolved.
 * @param path_baseoffset The path of the file or directory to operate on.
 * @param path_len
 * @param atim The desired values of the data access timestamp.
 * @param mtim The desired values of the data modification timestamp.
 * @param fstflags_extended A bitmask indicating which timestamps to adjust.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_filestat_set_times(__wasi_fd_t fd, __wasi_lookupflags_t flags,
                                                           __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                                           __wasi_timestamp_t atim, __wasi_timestamp_t mtim,
                                                           uint32_t fstflags_extended)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Create a hard link
 *
 * @param old_fd
 * @param old_flags Flags determining the method of how the path is resolved.
 * @param old_path_baseoffset The source path from which to link.
 * @param old_path_len
 * @param new_fd The working directory at which the resolution of the new path starts.
 * @param new_path_baseoffset The destination path at which to create the hard link.
 * @param new_path_len
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_link(__wasi_fd_t old_fd, __wasi_lookupflags_t old_flags,
                                             __wasi_size_t old_path_baseoffset, __wasi_size_t old_path_len,
                                             __wasi_fd_t new_fd, __wasi_size_t new_path_baseoffset,
                                             __wasi_size_t new_path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Open a file or directory
 * The returned file descriptor is not guaranteed to be the lowest-numbered
 * file descriptor not currently open; it is randomized to prevent
 * applications from depending on making assumptions about indexes, since this
 * is error-prone in multi-threaded contexts. The returned file descriptor is
 * guaranteed to be less than 2**31.
 *
 * @param dirfd
 * @param lookupflags
 * @param path_baseoffset
 * @param path_len
 * @param oflags
 * @param fs_rights_base
 * @param fs_rights_inheriting
 * @param fdflags
 * @param fd_retoffset The file descriptor of the file that has been opened.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_open(__wasi_fd_t dirfd, __wasi_lookupflags_t lookupflags,
                                             __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                             uint32_t oflags_extended, __wasi_rights_t fs_rights_base,
                                             __wasi_rights_t fs_rights_inheriting, uint32_t fdflags_extended,
                                             __wasi_size_t fd_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Remove a directory
 * Return `errno::notempty` if the directory is not empty.
 *
 * @param fd
 * @param path_baseoffset The path to a directory to remove.
 * @param path_len
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_readlink(__wasi_fd_t fd, __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                                 __wasi_size_t buf_baseretoffset, __wasi_size_t buf_len,
                                                 __wasi_size_t nread_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Remove a directory
 * Return `errno::notempty` if the directory is not empty.
 *
 * @param fd
 * @param path_baseoffset The path to a directory to remove.
 * @param path_len
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_remove_directory(__wasi_fd_t fd, __wasi_size_t path_baseoffset,
                                                         __wasi_size_t path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Rename a file or directory
 *
 * @param fd
 * @param old_path The source path of the file or directory to rename.
 * @param new_fd The working directory at which the resolution of the new path starts.
 * @param new_path The destination path to which to rename the file or directory.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_rename(__wasi_fd_t fd, __wasi_size_t old_path_baseoffset,
                                               __wasi_size_t old_path_len, __wasi_fd_t new_fd,
                                               __wasi_size_t new_path_baseoffset, __wasi_size_t new_path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Create a symbolic link
 *
 * @param old_path_baseoffset The contents of the symbolic link.
 * @param old_path_len
 * @param fd
 * @param new_path_baseoffset The path where we want the symbolic link.
 * @param new_path_len
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_symlink(__wasi_size_t old_path_baseoffset, __wasi_size_t old_path_len,
                                                __wasi_fd_t fd, __wasi_size_t new_path_baseoffset,
                                                __wasi_size_t new_path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Unlink a file
 * Return `errno::isdir` if the path refers to a directory.
 *
 * @param fd
 * @param path_baseoffset
 * @param path_len
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_path_unlink_file(__wasi_fd_t fd, __wasi_size_t path_baseoffset,
                                                    __wasi_size_t path_len)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Concurrently poll for the occurrence of a set of events.
 *
 * @param in The events to which to subscribe.
 * @param out The events that have occurred.
 * @param nsubscriptions Both the number of subscriptions and events.
 * @param retptr The number of events stored.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_poll_oneoff(__wasi_size_t in_baseoffset, __wasi_size_t out_baseoffset,
                                               __wasi_size_t nsubscriptions, __wasi_size_t nevents_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Terminate the process normally. An exit code of 0 indicates successful
 * termination of the program. The meanings of other values is dependent on
 * the environment.
 *
 * @param exitcode
 */
EXPORT void
sledge_abi__wasi_snapshot_preview1_proc_exit(__wasi_exitcode_t exitcode)
{
	struct sandbox *sandbox = current_sandbox_get();
	wasi_snapshot_preview1_backing_proc_exit(sandbox->wasi_context, exitcode);
}

/**
 * Send a signal to the process of the calling thread.
 *
 * @param sig_extended The signal condition to trigger.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_proc_raise(uint32_t sig_extended)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Write high-quality random data into a buffer.
 * This function blocks when the implementation is unable to immediately
 * provide sufficient high-quality random data.
 * This function may execute slowly, so when large mounts of random data are
 * required, it's advisable to use this function to seed a pseudo-random
 * number generator, rather than to provide the random data directly.
 *
 * @param buf_baseretoffset The buffer to fill with random data.
 * @param buf_len The length of the buffer
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_random_get(__wasi_size_t buf_baseretoffset, __wasi_size_t buf_len)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);
	uint8_t *buf_baseretptr = (uint8_t *)get_memory_ptr_for_runtime(buf_baseretoffset, buf_len);

	uint32_t rc = (uint32_t)wasi_snapshot_preview1_backing_random_get(sandbox->wasi_context, buf_baseretptr,
	                                                                  buf_len);
	sandbox_return(sandbox);

	return rc;
}

/**
 * Temporarily yield execution of the calling thread similar to `sched_yield` in POSIX.
 * This implementation ignores client calls and silently returns RC 0
 *
 * @return WASI_ESUCCESS
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_sched_yield(void)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Receive a message from a socket.
 * Note: This WASI syscall is in flux pending a decision on whether WASI
 * should only support fd_read and fd_write
 * See: https://github.com/WebAssembly/WASI/issues/4
 * Note: This is similar to `recv` in POSIX, though it also supports reading
 * the data into multiple buffers in the manner of `readv`.
 *
 * @param fd
 * @param ri_data_baseretoffset List of scatter/gather vectors to which to store data.
 * @param ri_data_len The length of the array pointed to by `ri_data`.
 * @param ri_flags Message flags.
 * @param ri_data_nbytes_retoffset Number of bytes stored in ri_data flags.
 * @param message_nbytes_retoffset Number of bytes stored in message flags.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_sock_recv(__wasi_fd_t fd, __wasi_size_t ri_data_baseretoffset,
                                             __wasi_size_t ri_data_len, uint32_t ri_flags_extended,
                                             __wasi_size_t ri_data_nbytes_retoffset,
                                             __wasi_size_t message_nbytes_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Send a message on a socket.
 * Note: This WASI syscall is in flux pending a decision on whether WASI
 * should only support fd_read and fd_write
 * See: https://github.com/WebAssembly/WASI/issues/4
 * Note: This is similar to `send` in POSIX, though it also supports writing
 * the data from multiple buffers in the manner of `writev`.
 *
 * @param fd
 * @param si_data_baseoffset List of scatter/gather vectors to which to retrieve data
 * @param si_data_len The length of the array pointed to by `si_data`.
 * @param si_flags Message flags.
 * @param nbytes_retoffset Number of bytes transmitted.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_sock_send(__wasi_fd_t fd, __wasi_size_t si_data_baseoffset,
                                             __wasi_size_t si_data_len, uint32_t si_flags_extended,
                                             __wasi_size_t nbytes_retoffset)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * Shut down socket send and receive channels.
 * Note: This WASI syscall is in flux pending a decision on whether WASI
 * should only support fd_read and fd_write
 * See: https://github.com/WebAssembly/WASI/issues/4
 *
 * @param fd
 * @param how Which channels on the socket to shut down.
 * @return status code
 */
EXPORT uint32_t
sledge_abi__wasi_snapshot_preview1_sock_shutdown(__wasi_fd_t fd, uint32_t how)
{
	return wasi_unsupported_syscall(__func__);
}

/**
 * @param key
 * @param key_len
 * @returns value_len at key or 0 if key not present
 */
EXPORT uint32_t
sledge_abi__scratch_storage_get_size(uint32_t key_offset, uint32_t key_len)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	uint8_t *key = (uint8_t *)get_memory_ptr_for_runtime(key_offset, key_len);

	uint32_t value_len;
	map_get(&sandbox->tenant->scratch_storage, key, key_len, &value_len);

	sandbox_return(sandbox);

	return value_len;
}

EXPORT int
sledge_abi__scratch_storage_get(uint32_t key_offset, uint32_t key_len, uint32_t buf_offset, uint32_t buf_len)
{
	int rc = 0;

	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	uint8_t *key = (uint8_t *)get_memory_ptr_for_runtime(key_offset, key_len);
	uint8_t *buf = (uint8_t *)get_memory_ptr_for_runtime(buf_offset, buf_len);

	uint32_t value_len;
	uint8_t *value = map_get(&sandbox->tenant->scratch_storage, key, key_len, &value_len);

	if (value == NULL) {
		rc = 1;
		goto DONE;
	} else if (value_len > buf_len) {
		rc = 2;
		goto DONE;
	} else {
		memcpy(buf, value, value_len);
		rc = 0;
	}

DONE:
	sandbox_return(sandbox);
	return rc;
}

/**
 * @param key_offset
 * @param key_len
 * @param value_offset
 * @param value_len
 * @returns 0 on success, 1 if already present,
 */
EXPORT int
sledge_abi__scratch_storage_set(uint32_t key_offset, uint32_t key_len, uint32_t value_offset, uint32_t value_len)
{
	int rc = 0;

	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	uint8_t *key   = (uint8_t *)get_memory_ptr_for_runtime(key_offset, key_len);
	uint8_t *value = (uint8_t *)get_memory_ptr_for_runtime(value_offset, value_len);

	bool did_set = map_set(&sandbox->tenant->scratch_storage, key, key_len, value, value_len);

DONE:
	sandbox_return(sandbox);
	return did_set ? 0 : 1;
}

/**
 * @param key_offset
 * @param key_len
 * @returns 0 on success, 1 if not present
 */
EXPORT int
sledge_abi__scratch_storage_delete(uint32_t key_offset, uint32_t key_len)
{
	int rc = 0;

	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	uint8_t *key = (uint8_t *)get_memory_ptr_for_runtime(key_offset, key_len);

	bool did_delete = map_delete(&sandbox->tenant->scratch_storage, key, key_len);

DONE:
	sandbox_return(sandbox);
	return did_delete ? 0 : 1;
}

/**
 * @param key_offset
 * @param key_len
 * @param value_offset
 * @param value_len
 */
EXPORT void
sledge_abi__scratch_storage_upsert(uint32_t key_offset, uint32_t key_len, uint32_t value_offset, uint32_t value_len)
{
	struct sandbox *sandbox = current_sandbox_get();

	sandbox_syscall(sandbox);

	uint8_t *key   = (uint8_t *)get_memory_ptr_for_runtime(key_offset, key_len);
	uint8_t *value = (uint8_t *)get_memory_ptr_for_runtime(value_offset, value_len);

	map_upsert(&sandbox->tenant->scratch_storage, key, key_len, value, value_len);

	sandbox_return(sandbox);
}
