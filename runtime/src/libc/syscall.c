/*
 * This code originally came from the aWsm compiler
 * It has since been updated
 * https://github.com/gwsystems/aWsm/blob/master/runtime/libc/libc_backing.c
 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "current_sandbox.h"

// What should we tell the child program its UID and GID are?
#define UID 0xFF
#define GID 0xFE

// Elf auxilary vector values (see google for what those are)
#define AT_NULL          0
#define AT_IGNORE        1
#define AT_EXECFD        2
#define AT_PHDR          3
#define AT_PHENT         4
#define AT_PHNUM         5
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
	// What program name will we put in the auxiliary vectors
	char *program_name = current_sandbox_get()->module->name;
	// Copy the program name into WASM accessible memory
	int32_t program_name_offset = offset;
	strcpy(get_memory_ptr_for_runtime(offset, sizeof(program_name)), program_name);
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
	memcpy(get_memory_ptr_for_runtime(env_vec_offset, sizeof(env_vec)), env_vec, sizeof(env_vec));

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
	/* Non-blocking copy on stdin */
	if (filedes == 0) {
		char *               buffer          = worker_thread_get_memory_ptr_void(buf_offset, nbyte);
		struct sandbox *     current_sandbox = current_sandbox_get();
		struct http_request *current_request = &current_sandbox->http_request;
		if (current_request->body_length <= 0) return 0;
		int bytes_to_read = nbyte > current_request->body_length ? current_request->body_length : nbyte;
		memcpy(buffer, current_request->body + current_request->body_read_length, bytes_to_read);
		current_request->body_read_length += bytes_to_read;
		current_request->body_length -= bytes_to_read;
		return bytes_to_read;
	}

	char *buf = worker_thread_get_memory_ptr_void(buf_offset, nbyte);

	int32_t res = 0;
	while (res < nbyte) {
		/* Read from the Socket */
		int32_t length_read = (int32_t)read(filedes, buf, nbyte);
		if (length_read < 0) {
			if (errno == EAGAIN)
				worker_thread_block_current_sandbox();
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
	if (fd == 1 || fd == 2) {
		char *          buffer = worker_thread_get_memory_ptr_void(buf_offset, buf_size);
		struct sandbox *s      = current_sandbox_get();
		int             l      = s->module->max_response_size - s->request_response_data_length;
		if (l > buf_size) l = buf_size;
		if (l == 0) return 0;
		memcpy(s->request_response_data + s->request_response_data_length, buffer, l);
		s->request_response_data_length += l;

		return l;
	}

	int   f   = current_sandbox_get_file_descriptor(fd);
	char *buf = worker_thread_get_memory_ptr_void(buf_offset, buf_size);

	int32_t res = 0;
	while (res < buf_size) {
		int32_t length_written = (int32_t)write(f, buf, buf_size);
		if (length_written < 0) {
			if (errno == EAGAIN)
				worker_thread_block_current_sandbox();
			else {
				/* All other errors */
				debuglog("Error reading socket %d - %s\n", fd, strerror(errno));
				goto err;
			}
		}

		res += length_written;
	}

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
#define WO_NOCTTY    0400
#define WO_TRUNC     01000
#define WO_APPEND    02000
#define WO_NONBLOCK  04000
#define WO_DSYNC     010000
#define WO_SYNC      04010000
#define WO_RSYNC     04010000
#define WO_DIRECTORY 0200000
#define WO_NOFOLLOW  0400000
#define WO_CLOEXEC   02000000


#define SYS_OPEN 2
int32_t
wasm_open(int32_t path_off, int32_t flags, int32_t mode)
{
	char *path = worker_thread_get_memory_string(path_off, MODULE_MAX_PATH_LENGTH);

	int iofd = current_sandbox_initialize_io_handle();
	if (iofd < 0) return -1;
	int32_t modified_flags = 0;

	if (flags & WO_RDONLY) {
		modified_flags |= O_RDONLY;
		// flags ^= WO_RDONLY;
	}

	if (flags & WO_WRONLY) {
		modified_flags |= O_WRONLY;
		// flags ^= WO_WRONLY;
	}

	if (flags & WO_RDWR) {
		modified_flags |= O_RDWR;
		// flags ^= WO_RDWR;
	}

	if (flags & WO_APPEND) {
		modified_flags |= O_APPEND;
		// flags ^= WO_APPEND;
	}

	if (flags & WO_CREAT) {
		modified_flags |= O_CREAT;
		// flags ^= WO_CREAT;
	}

	if (flags & WO_EXCL) {
		modified_flags |= O_EXCL;
		// flags ^= WO_EXCL;
	}

	int32_t res = (int32_t)open(path, modified_flags, mode);

	if (res == -1) return -errno;

	return res;
}

#define SYS_CLOSE 3
int32_t
wasm_close(int32_t io_handle_index)
{
	int     fd  = current_sandbox_get_file_descriptor(io_handle_index);
	int32_t res = (int32_t)close(fd);

	if (res == -1) return -errno;

	return res;
}

// What the wasm stat structure looks like
struct wasm_stat {
	int64_t  st_dev;
	uint64_t st_ino;
	uint32_t st_nlink;

	uint32_t st_mode;
	uint32_t st_uid;
	uint32_t st_gid;
	uint32_t __pad0;
	uint64_t st_rdev;
	uint64_t st_size;
	int32_t  st_blksize;
	int64_t  st_blocks;

	struct {
		int32_t tv_sec;
		int32_t tv_nsec;
	} st_atim;
	struct {
		int32_t tv_sec;
		int32_t tv_nsec;
	} st_mtim;
	struct {
		int32_t tv_sec;
		int32_t tv_nsec;
	} st_ctim;
	int32_t __pad1[3];
};

#define SYS_STAT 4

int32_t
wasm_stat(uint32_t path_str_offset, int32_t stat_offset)
{
	char *            path     = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	int32_t     res = lstat(path, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat){
		.st_dev     = stat.st_dev,
		.st_ino     = stat.st_ino,
		.st_nlink   = stat.st_nlink,
		.st_mode    = stat.st_mode,
		.st_uid     = stat.st_uid,
		.st_gid     = stat.st_gid,
		.st_rdev    = stat.st_rdev,
		.st_size    = stat.st_size,
		.st_blksize = stat.st_blksize,
		.st_blocks  = stat.st_blocks,
	};

	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;

	return res;
}

#define SYS_FSTAT 5
int32_t
wasm_fstat(int32_t filedes, int32_t stat_offset)
{
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	int32_t     res = fstat(filedes, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat){
		.st_dev     = stat.st_dev,
		.st_ino     = stat.st_ino,
		.st_nlink   = stat.st_nlink,
		.st_mode    = stat.st_mode,
		.st_uid     = stat.st_uid,
		.st_gid     = stat.st_gid,
		.st_rdev    = stat.st_rdev,
		.st_size    = stat.st_size,
		.st_blksize = stat.st_blksize,
		.st_blocks  = stat.st_blocks,
	};

	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;

	return res;
}

#define SYS_LSTAT 6
int32_t
wasm_lstat(int32_t path_str_offset, int32_t stat_offset)
{
	char *            path     = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	int32_t     res = lstat(path, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat){
		.st_dev     = stat.st_dev,
		.st_ino     = stat.st_ino,
		.st_nlink   = stat.st_nlink,
		.st_mode    = stat.st_mode,
		.st_uid     = stat.st_uid,
		.st_gid     = stat.st_gid,
		.st_rdev    = stat.st_rdev,
		.st_size    = stat.st_size,
		.st_blksize = stat.st_blksize,
		.st_blocks  = stat.st_blocks,
	};

	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;

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

	int32_t result = local_sandbox_context_cache.linear_memory_size;
	for (int i = 0; i < len / WASM_PAGE_SIZE; i++) { expand_memory(); }

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
	struct wasm_iovec *iov  = worker_thread_get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));
	for (int i = 0; i < iovcnt; i++) { read += wasm_read(fd, iov[i].base_offset, iov[i].len); }

	return read;
}

#define SYS_WRITEV 20
int32_t
wasm_writev(int32_t fd, int32_t iov_offset, int32_t iovcnt)
{
	struct sandbox *c = current_sandbox_get();
	if (fd == 1 || fd == 2) {
		// both 1 and 2 go to client.
		int                len = 0;
		struct wasm_iovec *iov = worker_thread_get_memory_ptr_void(iov_offset,
		                                                           iovcnt * sizeof(struct wasm_iovec));
		for (int i = 0; i < iovcnt; i++) {
			char *b = worker_thread_get_memory_ptr_void(iov[i].base_offset, iov[i].len);
			memcpy(c->request_response_data + c->request_response_data_length, b, iov[i].len);
			c->request_response_data_length += iov[i].len;
			len += iov[i].len;
		}

		return len;
	}

	// TODO: Implement below
	assert(0);


	struct wasm_iovec *iov = worker_thread_get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

	// If we aren't on MUSL, pass writev to printf if possible
#if defined(__GLIBC__)
	if (fd == 1) {
		int sum = 0;
		for (int i = 0; i < iovcnt; i++) {
			int32_t len = iov[i].len;
			void *  ptr = worker_thread_get_memory_ptr_void(iov[i].base_offset, len);

			printf("%.*s", len, ptr);
			sum += len;
		}
		return sum;
	}
#endif

	struct iovec vecs[iovcnt];
	for (int i = 0; i < iovcnt; i++) {
		int32_t len = iov[i].len;
		void *  ptr = worker_thread_get_memory_ptr_void(iov[i].base_offset, len);
		vecs[i]     = (struct iovec){ ptr, len };
	}

	int32_t res = (int32_t)writev(fd, vecs, iovcnt);
	if (res == -1) return -errno;

	return res;
}

#define SYS_MADVISE 28

#define SYS_GETPID 39
uint32_t
wasm_getpid()
{
	return (uint32_t)getpid();
}


#define WF_DUPFD 0
#define WF_GETFD 1
#define WF_SETFD 2
#define WF_GETFL 3
#define WF_SETFL 4

#define WF_SETOWN 8
#define WF_GETOWN 9
#define WF_SETSIG 10
#define WF_GETSIG 11

#define WF_GETLK  5
#define WF_SETLK  6
#define WF_SETLKW 7

#define SYS_FCNTL 72
uint32_t
wasm_fcntl(uint32_t fd, uint32_t cmd, uint32_t arg_or_lock_ptr)
{
	switch (cmd) {
	case WF_SETFD:
		//            return fcntl(fd, F_SETFD, arg_or_lock_ptr);
		return 0;
	case WF_SETLK:
		return 0;
	default:
		panic("Unexpected Command");
	}
}

#define SYS_FSYNC 74
uint32_t
wasm_fsync(uint32_t filedes)
{
	uint32_t res = fsync(filedes);
	if (res == -1) return -errno;

	return 0;
}

#define SYS_GETCWD 79
uint32_t
wasm_getcwd(uint32_t buf_offset, uint32_t buf_size)
{
	char *buf = worker_thread_get_memory_ptr_void(buf_offset, buf_size);
	char *res = getcwd(buf, buf_size);

	if (!res) return 0;
	return buf_offset;
}

#define SYS_UNLINK 87
uint32_t
wasm_unlink(uint32_t path_str_offset)
{
	char *   str = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	uint32_t res = unlink(str);
	if (res == -1) return -errno;

	return 0;
}

#define SYS_GETEUID 107
uint32_t
wasm_geteuid()
{
	return (uint32_t)geteuid();
}

#define SYS_SET_THREAD_AREA 205

#define SYS_SET_TID_ADDRESS 218

#define SYS_GET_TIME 228
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

	struct wasm_time_spec *timespec = worker_thread_get_memory_ptr_void(timespec_off,
	                                                                    sizeof(struct wasm_time_spec));

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
	exit(status);
	return 0;
}

#define SYS_FCHOWN 93
int32_t
wasm_fchown(int32_t fd, uint32_t owner, uint32_t group)
{
	return fchown(fd, owner, group);
}

// networking syscalls
#define SYS_SOCKET  41
#define SYS_CONNECT 42
#define SYS_ACCEPT  43
#define SYS_BIND    49
#define SYS_LISTEN  50
int32_t
wasm_socket(int32_t domain, int32_t type, int32_t protocol)
{
	return socket(domain, type, protocol);
}

int32_t
wasm_connect(int32_t sockfd, int32_t sockaddr_offset, int32_t addrlen)
{
	return connect(sockfd, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen), addrlen);
}

int32_t
wasm_accept(int32_t sockfd, int32_t sockaddr_offset, int32_t addrlen_offset)
{
	socklen_t *addrlen = worker_thread_get_memory_ptr_void(addrlen_offset, sizeof(socklen_t));

	return accept(sockfd, worker_thread_get_memory_ptr_void(sockaddr_offset, *addrlen), addrlen);
}

int32_t
wasm_bind(int32_t sockfd, int32_t sockaddr_offset, int32_t addrlen)
{
	return bind(sockfd, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen), addrlen);
}

int32_t
wasm_listen(int32_t sockfd, int32_t backlog)
{
	return listen(sockfd, backlog);
}

#define SYS_SENDTO   44
#define SYS_RECVFROM 45

int32_t
wasm_sendto(int32_t fd, int32_t buff_offset, int32_t len, int32_t flags, int32_t sockaddr_offset, int32_t sockaddr_len)
{
	char *           buf  = worker_thread_get_memory_ptr_void(buff_offset, len);
	struct sockaddr *addr = sockaddr_len ? worker_thread_get_memory_ptr_void(sockaddr_offset, sockaddr_len) : NULL;

	return sendto(fd, buf, len, flags, addr, sockaddr_len);
}

int32_t
wasm_recvfrom(int32_t fd, int32_t buff_offset, int32_t size, int32_t flags, int32_t sockaddr_offset,
              int32_t socklen_offset)
{
	char *           buf  = worker_thread_get_memory_ptr_void(buff_offset, size);
	socklen_t *      len  = worker_thread_get_memory_ptr_void(socklen_offset, sizeof(socklen_t));
	struct sockaddr *addr = *len ? worker_thread_get_memory_ptr_void(sockaddr_offset, *len) : NULL;

	return recvfrom(fd, buf, size, flags, addr, addr ? len : NULL);
}

int32_t
inner_syscall_handler(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
	int32_t res;
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
	case SYS_MADVISE:
		/* Note: These are called, but are unimplemented and fail silently */
		return 0;
	case SYS_RT_SIGACTION:
	case SYS_RT_SIGPROGMASK:
	default:
		/* This is a general catch all for the other functions below */
		debuglog("Call to unknown or implemented syscall %d\n", n);
		errno = ENOSYS;
		return -1;

		/* TODO: The calls below need to be validated / refactored to be non-blocking */
		// case SYS_OPEN:
		// 	return wasm_open(a, b, c);
		// case SYS_STAT:
		// 	return wasm_stat(a, b);
		// case SYS_FSTAT:
		// 	return wasm_fstat(a, b);
		// case SYS_LSTAT:
		// 	return wasm_lstat(a, b);
		// case SYS_LSEEK:
		// 	return wasm_lseek(a, b, c);
		// case SYS_GETPID:
		// 	return wasm_getpid();
		// case SYS_FCNTL:
		// 	return wasm_fcntl(a, b, c);
		// case SYS_FSYNC:
		// 	return wasm_fsync(a);
		// case SYS_UNLINK:
		// 	return wasm_unlink(a);
		// case SYS_GETCWD:
		// 	return wasm_getcwd(a, b);
		// case SYS_GETEUID:
		// 	return wasm_geteuid();
		// case SYS_FCHOWN:
		// 	return wasm_fchown(a, b, c);
		// case SYS_SOCKET:
		// 	return wasm_socket(a, b, c);
		// case SYS_CONNECT:
		// 	return wasm_connect(a, b, c);
		// case SYS_ACCEPT:
		// 	return wasm_accept(a, b, c);
		// case SYS_BIND:
		// 	return wasm_bind(a, b, c);
		// case SYS_LISTEN:
		// 	return wasm_listen(a, b);
		// case SYS_SENDTO:
		// 	return wasm_sendto(a, b, c, d, e, f);
		// case SYS_RECVFROM:
		// 	return wasm_recvfrom(a, b, c, d, e, f);
	}
	printf("syscall %d (%d, %d, %d, %d, %d, %d)\n", n, a, b, c, d, e, f);
	assert(0);

	return 0;
}
