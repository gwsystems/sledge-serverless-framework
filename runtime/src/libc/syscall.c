/*
 * This code originally came from the aWsm compiler
 * It has since been updated
 * https://github.com/gwsystems/aWsm/blob/master/runtime/libc/libc_backing.c
 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "current_sandbox.h"
#include "scheduler.h"
#include "sandbox_functions.h"
#include "worker_thread.h"
#include "wasm_module_instance.h"

// What should we tell the child program its UID and GID are?
#define UID 0xFF
#define GID 0xFE

// Elf auxilary vector values (see google for what those are)
#define AT_PHENT         4
#define AT_PAGESZ        6
#define AT_BASE          7
#define AT_FLAGS         8
#define AT_ENTRY         9
#define AT_NOTELF        10
#define AT_UID           11
#define AT_EUID          12
#define AT_GID           13
#define AT_EGID          14
#define AT_CLKTCK        17
#define AT_SECURE        23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM        25

// offset = a WASM ptr to memory the runtime can use
void
stub_init(int32_t offset)
{
	struct sandbox *current_sandbox = current_sandbox_get();
	// What program name will we put in the auxiliary vectors
	char *program_name = current_sandbox->module->name;
	// Copy the program name into WASM accessible memory
	int32_t program_name_offset = offset;
	strcpy(wasm_memory_get_ptr_void(current_sandbox->memory, offset, sizeof(program_name)), program_name);
	offset += sizeof(program_name);

	// The construction of this is:
	// evn1, env2, ..., NULL, auxv_n1, auxv_1, auxv_n2, auxv_2 ..., NULL
	int32_t env_vec[] = {
		// Env variables would live here, but we don't supply any
		0,
		// We supply only the bare minimum AUX vectors
		AT_PAGESZ,
		WASM_PAGE_SIZE,
		AT_UID,
		UID,
		AT_EUID,
		UID,
		AT_GID,
		GID,
		AT_EGID,
		GID,
		AT_SECURE,
		0,
		AT_RANDOM,
		(int32_t)rand(), // It's pretty stupid to use rand here, but w/e
		0,
	};
	int32_t env_vec_offset = offset;
	memcpy(wasm_memory_get_ptr_void(current_sandbox->memory, env_vec_offset, sizeof(env_vec)), env_vec,
	       sizeof(env_vec));

	module_initialize_libc(current_sandbox_get()->module, env_vec_offset, program_name_offset);
}

// Emulated syscall implementations

// We define our own syscall numbers, because WASM uses x86_64 values even on systems that are not x86_64
#define SYS_READ 0

/**
 * @param filedes to read
 * @param buf_offset offset into wasm linear memory
 * @param nbyte number of bytes to read
 * @returns returns bytes read or -errno
 */
uint32_t
wasm_read(int32_t filedes, int32_t buf_offset, int32_t nbyte)
{
	struct sandbox *current_sandbox = current_sandbox_get();

	/* Non-blocking copy on stdin */
	if (filedes == 0) {
		char *               buffer          = current_sandbox_get_ptr_void(buf_offset, nbyte);
		struct http_request *current_request = &current_sandbox->http_request;
		if (current_request->body_length <= 0) return 0;
		int bytes_to_read = nbyte > current_request->body_length ? current_request->body_length : nbyte;
		memcpy(buffer, current_request->body + current_request->body_read_length, bytes_to_read);
		current_request->body_read_length += bytes_to_read;
		current_request->body_length -= bytes_to_read;
		return bytes_to_read;
	}

	char *buf = current_sandbox_get_ptr_void(buf_offset, nbyte);

	int32_t res = 0;
	while (res < nbyte) {
		/* Read from the Socket */
		int32_t length_read = (int32_t)read(filedes, buf, nbyte);
		if (length_read < 0) {
			if (errno == EAGAIN)
				current_sandbox_sleep();
			else {
				/* All other errors */
				debuglog("Error reading socket %d - %s\n", filedes, strerror(errno));
				goto err;
			}
		}

		res += length_read;
	}

done:
	return res;
err:
	res = -errno;
	goto done;
}

#define SYS_WRITE 1
int32_t
wasm_write(int32_t fd, int32_t buf_offset, int32_t buf_size)
{
	struct sandbox *s        = current_sandbox_get();
	char *          buffer   = current_sandbox_get_ptr_void(buf_offset, buf_size);
	struct vec_u8 * response = &s->response;

	if (fd == STDERR_FILENO) { write(STDERR_FILENO, buffer, buf_size); }

	if (fd == STDOUT_FILENO) {
		int buffer_remaining = response->capacity - response->length;
		int to_write         = buffer_remaining > buf_size ? buf_size : buffer_remaining;

		if (to_write == 0) return 0;
		memcpy(&response->buffer[response->length], buffer, to_write);
		response->length += to_write;

		return to_write;
	}

	int res = ENOTSUP;

done:
	return res;
err:
	res = -errno;
	goto done;
}

#define WO_RDONLY    00
#define WO_WRONLY    01
#define WO_RDWR      02
#define WO_CREAT     0100
#define WO_EXCL      0200
#define WO_APPEND    02000
#define WO_RSYNC     04010000
#define WO_DIRECTORY 0200000
#define WO_NOFOLLOW  0400000
#define WO_CLOEXEC   02000000


#define SYS_OPEN 2
int32_t
wasm_open(int32_t path_off, int32_t flags, int32_t mode)
{
	char *path = current_sandbox_get_string(path_off, MODULE_MAX_PATH_LENGTH);

	int res = ENOTSUP;

	return res;
}

#define SYS_CLOSE 3
int32_t
wasm_close(int32_t fd)
{
	// Silently disregard client requests to close STDIN, STDOUT, or STDERR
	if (fd <= STDERR_FILENO) return 0;

	int res = ENOTSUP;

	return res;
}

#define SYS_LSEEK 8
int32_t
wasm_lseek(int32_t filedes, int32_t file_offset, int32_t whence)
{
	int32_t res = (int32_t)lseek(filedes, file_offset, whence);

	if (res == -1) return -errno;

	return res;
}

#define SYS_MMAP 9
uint32_t
wasm_mmap(int32_t addr, int32_t len, int32_t prot, int32_t flags, int32_t fd, int32_t offset)
{
	if (addr != 0) {
		printf("parameter void *addr is not supported!\n");
		assert(0);
	}

	if (fd != -1) {
		printf("file mapping is not supported!\n");
		assert(0);
	}

	assert(len % WASM_PAGE_SIZE == 0);

	int32_t result = wasm_memory_get_size(current_wasm_module_instance.memory);
	if (wasm_memory_expand(current_wasm_module_instance.memory, len) == -1) { result = (uint32_t)-1; }

	return result;
}

#define SYS_MUNMAP 11

#define SYS_BRK 12

#define SYS_RT_SIGACTION 13

#define SYS_RT_SIGPROGMASK 14

#define SYS_IOCTL 16
int32_t
wasm_ioctl(int32_t fd, int32_t request, int32_t data_offet)
{
	// musl libc does some ioctls to stdout, so just allow these to silently go through
	// FIXME: The above is idiotic
	return 0;
}

#define SYS_READV 19
struct wasm_iovec {
	int32_t base_offset;
	int32_t len;
};

int32_t
wasm_readv(int32_t fd, int32_t iov_offset, int32_t iovcnt)
{
	int32_t            read = 0;
	struct wasm_iovec *iov  = current_sandbox_get_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));
	for (int i = 0; i < iovcnt; i++) { read += wasm_read(fd, iov[i].base_offset, iov[i].len); }

	return read;
}

#define SYS_WRITEV 20
int32_t
wasm_writev(int32_t fd, int32_t iov_offset, int32_t iovcnt)
{
	struct sandbox *s = current_sandbox_get();
	if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
		// both 1 and 2 go to client.
		int                len = 0;
		struct wasm_iovec *iov = current_sandbox_get_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));
		for (int i = 0; i < iovcnt; i++) { len += wasm_write(fd, iov[i].base_offset, iov[i].len); }

		return len;
	}

	// TODO: Implement below
	assert(0);


	struct wasm_iovec *iov = current_sandbox_get_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

	// If we aren't on MUSL, pass writev to printf if possible
#if defined(__GLIBC__)
	if (fd == 1) {
		int sum = 0;
		for (int i = 0; i < iovcnt; i++) {
			int32_t len = iov[i].len;
			void *  ptr = current_sandbox_get_ptr_void(iov[i].base_offset, len);

			printf("%.*s", len, (char *)ptr);
			sum += len;
		}
		return sum;
	}
#endif

	struct iovec vecs[iovcnt];
	for (int i = 0; i < iovcnt; i++) {
		int32_t len = iov[i].len;
		void *  ptr = current_sandbox_get_ptr_void(iov[i].base_offset, len);
		vecs[i]     = (struct iovec){ ptr, len };
	}

	int32_t res = (int32_t)writev(fd, vecs, iovcnt);
	if (res == -1) return -errno;

	return res;
}

#define SYS_MREMAP 25
int32_t
wasm_mremap(int32_t offset, int32_t old_size, int32_t new_size, int32_t flags)
{
	assert(offset >= 0);
	assert(offset + old_size <= INT32_MAX);

	// We do not implement compaction yet, so just return immediately if shrinking
	if (new_size <= old_size) return offset;

	// If at end of linear memory, just expand and return same address
	if (offset + old_size == current_wasm_module_instance.memory->size) {
		int32_t amount_to_expand = new_size - old_size;
		wasm_memory_expand(current_wasm_module_instance.memory, amount_to_expand);
		return offset;
	}

	// Otherwise allocate at end of address space and copy
	int32_t new_offset = current_wasm_module_instance.memory->size;
	wasm_memory_expand(current_wasm_module_instance.memory, new_size);

	// Get pointer of old offset and pointer of new offset
	uint8_t *linear_mem = current_wasm_module_instance.memory->data;
	uint8_t *src        = &linear_mem[offset];
	uint8_t *dest       = &linear_mem[new_offset];

	// Copy Values. We can use memcpy because we don't overlap
	memcpy((void *)dest, (void *)src, old_size);

	return new_offset;
}

#define SYS_MADVISE 28

#define SYS_GETPID 39
uint32_t
wasm_getpid()
{
	return (uint32_t)getpid();
}


#define WF_SETFD            2
#define WF_GETSIG           11
#define WF_SETLK            6
#define WF_SETLKW           7
#define SYS_SET_THREAD_AREA 205
#define SYS_SET_TID_ADDRESS 218
#define SYS_GET_TIME        228
struct wasm_time_spec {
	uint64_t sec;
	uint32_t nanosec;
};

int32_t
wasm_get_time(int32_t clock_id, int32_t timespec_off)
{
	clockid_t real_clock;
	switch (clock_id) {
	case 0:
		real_clock = CLOCK_REALTIME;
		break;
	case 1:
		real_clock = CLOCK_MONOTONIC;
		break;
	case 2:
		real_clock = CLOCK_PROCESS_CPUTIME_ID;
		break;
	default:
		assert(0);
	}

	struct wasm_time_spec *timespec = current_sandbox_get_ptr_void(timespec_off, sizeof(struct wasm_time_spec));

	struct timespec native_timespec = { 0, 0 };
	int             res             = clock_gettime(real_clock, &native_timespec);
	if (res == -1) return -errno;

	timespec->sec     = native_timespec.tv_sec;
	timespec->nanosec = native_timespec.tv_nsec;

	return res;
}

#define SYS_EXIT       60
#define SYS_EXIT_GROUP 231
int32_t
wasm_exit_group(int32_t status)
{
	debuglog("Called wasm_exit_group");
	// I believe that if a sandbox called this, it would cause the runtime to exit
	// exit(status);
	return 0;
}

int32_t
inner_syscall_handler(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
	switch (n) {
	case SYS_READ:
		return wasm_read(a, b, c);
	case SYS_WRITE:
		return wasm_write(a, b, c);
	case SYS_WRITEV:
		return wasm_writev(a, b, c);
	case SYS_CLOSE:
		return wasm_close(a);
	case SYS_LSEEK:
		return wasm_lseek(a, b, c);
	case SYS_EXIT:
	case SYS_EXIT_GROUP:
		return wasm_exit_group(a);
	case SYS_MMAP:
		return wasm_mmap(a, b, c, d, e, f);
	case SYS_GET_TIME:
		return wasm_get_time(a, b);
	case SYS_READV:
		return wasm_readv(a, b, c);
	case SYS_MUNMAP:
	case SYS_IOCTL:
	case SYS_SET_THREAD_AREA:
	case SYS_SET_TID_ADDRESS:
	case SYS_BRK:
	case SYS_MREMAP:
		return wasm_mremap(a, b, c, b);
	case SYS_MADVISE:
		/* Note: These are called, but are unimplemented and fail silently */
		return 0;
	case SYS_RT_SIGACTION:
	case SYS_RT_SIGPROGMASK:
	default:
		/* This is a general catch all for the other functions below */
		debuglog("Call to unknown or implemented syscall %d\n", n);
		debuglog("syscall %d (%d, %d, %d, %d, %d, %d)\n", n, a, b, c, d, e, f);
		errno = ENOSYS;
		return -1;
	}
}
