#include <uv.h>

#include "current_sandbox.h"
#include "http_request.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "types.h"
#include "worker_thread.h"

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
stub_init(i32 offset)
{
	// What program name will we put in the auxiliary vectors
	char *program_name = current_sandbox_get()->module->name;
	// Copy the program name into WASM accessible memory
	i32 program_name_offset = offset;
	strcpy(get_memory_ptr_for_runtime(offset, sizeof(program_name)), program_name);
	offset += sizeof(program_name);

	// The construction of this is:
	// evn1, env2, ..., NULL, auxv_n1, auxv_1, auxv_n2, auxv_2 ..., NULL
	i32 env_vec[] = {
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
		(i32)rand(), // It's pretty stupid to use rand here, but w/e
		0,
	};
	i32 env_vec_offset = offset;
	memcpy(get_memory_ptr_for_runtime(env_vec_offset, sizeof(env_vec)), env_vec, sizeof(env_vec));

	module_initialize_libc(current_sandbox_get()->module, env_vec_offset, program_name_offset);
}

// Emulated syscall implementations
static inline void *
uv_fs_get_data(uv_fs_t *req)
{
	return req->data;
}

static inline ssize_t
uv_fs_get_result(uv_fs_t *req)
{
	return req->result;
}

static inline uv_fs_type
uv_fs_get_type(uv_fs_t *req)
{
	return req->fs_type;
}

#define UV_FS_REQ_INIT()                                   \
	{                                                  \
		.data = current_sandbox_get(), .result = 0 \
	}

static void
wasm_fs_callback(uv_fs_t *req)
{
	debuglog("[%p]\n", req->data);
	worker_thread_wakeup_sandbox((sandbox_t *)req->data);
}

// We define our own syscall numbers, because WASM uses x86_64 values even on systems that are not x86_64
#define SYS_READ 0
uint32_t
wasm_read(i32 filedes, i32 buf_offset, i32 nbyte)
{
	if (filedes == 0) {
		char *               buffer = worker_thread_get_memory_ptr_void(buf_offset, nbyte);
		struct sandbox *     s      = current_sandbox_get();
		struct http_request *r      = &s->http_request;
		if (r->body_length <= 0) return 0;
		int l = nbyte > r->body_length ? r->body_length : nbyte;
		memcpy(buffer, r->body + r->body_read_length, l);
		r->body_read_length += l;
		r->body_length -= l;
		return l;
	}
	int f = current_sandbox_get_file_descriptor(filedes);
	// TODO: read on other file types
	uv_fs_t req    = UV_FS_REQ_INIT();
	char *  buffer = worker_thread_get_memory_ptr_void(buf_offset, nbyte);

	debuglog("[%p] start[%d:%d, n%d]\n", uv_fs_get_data(&req), filedes, f, nbyte);
	uv_buf_t bufv = uv_buf_init(buffer, nbyte);
	uv_fs_read(worker_thread_get_libuv_handle(), &req, f, &bufv, 1, -1, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);

	return ret;
}

#define SYS_WRITE 1
i32
wasm_write(i32 file_descriptor, i32 buf_offset, i32 buf_size)
{
	if (file_descriptor == 1 || file_descriptor == 2) {
		char *          buffer = worker_thread_get_memory_ptr_void(buf_offset, buf_size);
		struct sandbox *s      = current_sandbox_get();
		int             l      = s->module->max_response_size - s->request_response_data_length;
		if (l > buf_size) l = buf_size;
		if (l == 0) return 0;
		memcpy(s->request_response_data + s->request_response_data_length, buffer, l);
		s->request_response_data_length += l;

		return l;
	}
	int f = current_sandbox_get_file_descriptor(file_descriptor);
	// TODO: read on other file types
	uv_fs_t req    = UV_FS_REQ_INIT();
	char *  buffer = worker_thread_get_memory_ptr_void(buf_offset, buf_size);

	uv_buf_t bufv = uv_buf_init(buffer, buf_size);
	uv_fs_write(worker_thread_get_libuv_handle(), &req, f, &bufv, 1, -1, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	uv_fs_req_cleanup(&req);

	return ret;
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
i32
wasm_open(i32 path_off, i32 flags, i32 mode)
{
	uv_fs_t req  = UV_FS_REQ_INIT();
	char *  path = worker_thread_get_memory_string(path_off, MODULE_MAX_PATH_LENGTH);

	int iofd = current_sandbox_initialize_io_handle();
	if (iofd < 0) return -1;
	i32 modified_flags = 0;
	if (flags & WO_RDONLY) modified_flags |= O_RDONLY;
	if (flags & WO_WRONLY) modified_flags |= O_WRONLY;
	if (flags & WO_RDWR) modified_flags |= O_RDWR;
	if (flags & WO_APPEND) modified_flags |= O_APPEND;
	if (flags & WO_CREAT) modified_flags |= O_CREAT;
	if (flags & WO_EXCL) modified_flags |= O_EXCL;
	debuglog("[%p] start[%s:%d:%d]\n", uv_fs_get_data(&req), path, flags, modified_flags);

	uv_fs_open(worker_thread_get_libuv_handle(), &req, path, modified_flags, mode, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);
	if (ret < 0)
		current_sandbox_close_file_descriptor(iofd);
	else
		current_sandbox_set_file_descriptor(iofd, ret);

	return iofd;
}

#define SYS_CLOSE 3
i32
wasm_close(i32 file_descriptor)
{
	if (file_descriptor >= 0 && file_descriptor <= 2) { return 0; }
	struct sandbox *     c    = current_sandbox_get();
	int                  d    = current_sandbox_get_file_descriptor(file_descriptor);
	union uv_any_handle *h    = current_sandbox_get_libuv_handle(file_descriptor);
	uv_handle_type       type = ((uv_handle_t *)h)->type;
	debuglog("[%p] [%d,%d]\n", c, file_descriptor, d);

	if (type == UV_TCP) {
		debuglog("[%p] close tcp\n", c);
		// TODO: close!
		return 0;
	} else if (type == UV_UDP) {
		debuglog("[%p] close udp\n", c);
		// TODO: close!
		return 0;
	}

	uv_fs_t req = UV_FS_REQ_INIT();
	debuglog("[%p] file[%d,%d]\n", uv_fs_get_data(&req), file_descriptor, d);
	uv_fs_close(worker_thread_get_libuv_handle(), &req, d, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);
	if (ret == 0) current_sandbox_close_file_descriptor(file_descriptor);
	return ret;
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
	i32      st_blksize;
	int64_t  st_blocks;

	struct {
		i32 tv_sec;
		i32 tv_nsec;
	} st_atim;
	struct {
		i32 tv_sec;
		i32 tv_nsec;
	} st_mtim;
	struct {
		i32 tv_sec;
		i32 tv_nsec;
	} st_ctim;
	i32 __pad1[3];
};

#define SYS_STAT 4
// What the OSX stat structure looks like:
//     struct stat { /* when _DARWIN_FEATURE_64_BIT_INODE is NOT defined */
//         dev_t    st_dev;    /* device inode resides on */
//         ino_t    st_ino;    /* inode's number */
//         mode_t   st_mode;   /* inode protection mode */
//         nlink_t  st_nlink;  /* number of hard links to the file */
//         uid_t    st_uid;    /* user-id of owner */
//         gid_t    st_gid;    /* group-id of owner */
//         dev_t    st_rdev;   /* device type, for special file inode */
//         struct timespec st_atimespec;  /* time of last access */
//         struct timespec st_mtimespec;  /* time of last data modification */
//         struct timespec st_ctimespec;  /* time of last file status change */
//         off_t    st_size;   /* file size, in bytes */
//         quad_t   st_blocks; /* blocks allocated for file */
//         u_long   st_blksize;/* optimal file sys I/O ops blocksize */
//         u_long   st_flags;  /* user defined flags for file */
//         u_long   st_gen;    /* file generation number */
//     };

i32
wasm_stat(uint32_t path_str_offset, i32 stat_offset)
{
	char *            path     = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	i32         res = lstat(path, &stat);
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
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec  = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}

#define SYS_FSTAT 5
i32
wasm_fstat(i32 filedes, i32 stat_offset)
{
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	int         d   = current_sandbox_get_file_descriptor(filedes);
	i32         res = fstat(d, &stat);
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
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec  = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}

#define SYS_LSTAT 6
i32
wasm_lstat(i32 path_str_offset, i32 stat_offset)
{
	char *            path     = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	struct wasm_stat *stat_ptr = worker_thread_get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	i32         res = lstat(path, &stat);
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
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec  = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec  = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec  = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec  = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}


#define SYS_LSEEK 8
i32
wasm_lseek(i32 filedes, i32 file_offset, i32 whence)
{
	int d   = current_sandbox_get_file_descriptor(filedes);
	i32 res = (i32)lseek(d, file_offset, whence);

	if (res == -1) return -errno;

	return res;
}

#define SYS_MMAP 9
uint32_t
wasm_mmap(i32 addr, i32 len, i32 prot, i32 flags, i32 file_descriptor, i32 offset)
{
	int d = current_sandbox_get_file_descriptor(file_descriptor);
	if (file_descriptor >= 0) assert(d >= 0);
	if (addr != 0) panic("parameter void *addr is not supported!\n");
	if (d != -1) panic("file mapping is not supported!\n");

	assert(len % WASM_PAGE_SIZE == 0);

	i32 result = local_sandbox_context_cache.linear_memory_size;
	for (int i = 0; i < len / WASM_PAGE_SIZE; i++) { expand_memory(); }

	return result;
}

#define SYS_MUNMAP 11

#define SYS_BRK 12

#define SYS_RT_SIGACTION 13

#define SYS_RT_SIGPROGMASK 14

#define SYS_IOCTL 16
i32
wasm_ioctl(i32 file_descriptor, i32 request, i32 data_offet)
{
	// int d = current_sandbox_get_file_descriptor(file_descriptor);
	// musl libc does some ioctls to stdout, so just allow these to silently go through
	// FIXME: The above is idiotic
	// assert(d == 1);
	return 0;
}

#define SYS_READV 19
struct wasm_iovec {
	i32 base_offset;
	i32 len;
};

i32
wasm_readv(i32 file_descriptor, i32 iov_offset, i32 iovcnt)
{
	if (file_descriptor == 0) {
		// both 1 and 2 go to client.
		int                  len = 0;
		struct wasm_iovec *  iov = worker_thread_get_memory_ptr_void(iov_offset,
                                                                           iovcnt * sizeof(struct wasm_iovec));
		struct sandbox *     s   = current_sandbox_get();
		struct http_request *r   = &s->http_request;
		if (r->body_length <= 0) return 0;
		for (int i = 0; i < iovcnt; i++) {
			int l = iov[i].len > r->body_length ? r->body_length : iov[i].len;
			if (l <= 0) break;
			char *b = worker_thread_get_memory_ptr_void(iov[i].base_offset, iov[i].len);
			// http request body!
			memcpy(b, r->body + r->body_read_length + len, l);
			len += l;
			r->body_length -= l;
		}
		r->body_read_length += len;

		return len;
	}
	// TODO: read on other file types
	int                gret = 0;
	int                d    = current_sandbox_get_file_descriptor(file_descriptor);
	struct wasm_iovec *iov  = worker_thread_get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

	for (int i = 0; i < iovcnt; i += RUNTIME_READ_WRITE_VECTOR_LENGTH) {
		uv_fs_t  req                                    = UV_FS_REQ_INIT();
		uv_buf_t bufs[RUNTIME_READ_WRITE_VECTOR_LENGTH] = { 0 }; // avoid mallocs here!
		int      j                                      = 0;

		for (j = 0; j < RUNTIME_READ_WRITE_VECTOR_LENGTH && i + j < iovcnt; j++) {
			bufs[j] = uv_buf_init(worker_thread_get_memory_ptr_void(iov[i + j].base_offset, iov[i + j].len),
			                      iov[i + j].len);
		}
		debuglog("[%p] start[%d,%d, n%d:%d]\n", uv_fs_get_data(&req), file_descriptor, d, i, j);
		uv_fs_read(worker_thread_get_libuv_handle(), &req, d, bufs, j, -1, wasm_fs_callback);
		worker_thread_block_current_sandbox();

		int ret = uv_fs_get_result(&req);
		debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
		uv_fs_req_cleanup(&req);
		if (ret < 0) return ret;
		gret += ret;
	}
	debuglog(" gend[%d]\n", gret);

	return gret;
}

#define SYS_WRITEV 20
i32
wasm_writev(i32 file_descriptor, i32 iov_offset, i32 iovcnt)
{
	struct sandbox *c = current_sandbox_get();
	if (file_descriptor == 1 || file_descriptor == 2) {
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
	// TODO: read on other file types
	int                d    = current_sandbox_get_file_descriptor(file_descriptor);
	int                gret = 0;
	struct wasm_iovec *iov  = worker_thread_get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

	for (int i = 0; i < iovcnt; i += RUNTIME_READ_WRITE_VECTOR_LENGTH) {
		uv_fs_t  req                                    = UV_FS_REQ_INIT();
		uv_buf_t bufs[RUNTIME_READ_WRITE_VECTOR_LENGTH] = { 0 }; // avoid mallocs here!
		int      j                                      = 0;

		for (j = 0; j < RUNTIME_READ_WRITE_VECTOR_LENGTH && i + j < iovcnt; j++) {
			bufs[j] = uv_buf_init(worker_thread_get_memory_ptr_void(iov[i + j].base_offset, iov[i + j].len),
			                      iov[i + j].len);
		}
		debuglog("[%p] start[%d,%d, n%d:%d]\n", uv_fs_get_data(&req), file_descriptor, d, i, j);
		uv_fs_write(worker_thread_get_libuv_handle(), &req, d, bufs, j, -1, wasm_fs_callback);
		worker_thread_block_current_sandbox();

		int ret = uv_fs_get_result(&req);
		debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
		uv_fs_req_cleanup(&req);
		if (ret < 0) return ret;
		gret += ret;
	}
	debuglog(" gend[%d]\n", gret);

	return gret;
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
wasm_fcntl(uint32_t file_descriptor, uint32_t cmd, uint32_t arg_or_lock_ptr)
{
	int d = current_sandbox_get_file_descriptor(file_descriptor);
	switch (cmd) {
	case WF_SETFD:
		//            return fcntl(d, F_SETFD, arg_or_lock_ptr);
		return 0;
	case WF_SETLK:
		return 0;
	default:
		assert(0);
	}
}

#define SYS_FSYNC 74
uint32_t
wasm_fsync(uint32_t file_descriptor)
{
	int     d   = current_sandbox_get_file_descriptor(file_descriptor);
	uv_fs_t req = UV_FS_REQ_INIT();
	debuglog("[%p] start[%d,%d]\n", uv_fs_get_data(&req), file_descriptor, d);
	uv_fs_fsync(worker_thread_get_libuv_handle(), &req, d, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);

	return ret;
}

#define SYS_GETCWD 79
uint32_t
wasm_getcwd(uint32_t buf_offset, uint32_t buf_size)
{
	char *buffer = worker_thread_get_memory_ptr_void(buf_offset, buf_size);
	char *res    = getcwd(buffer, buf_size);

	if (!res) return 0;
	return buf_offset;
}

#define SYS_UNLINK 87
uint32_t
wasm_unlink(uint32_t path_str_offset)
{
	char *  path = worker_thread_get_memory_string(path_str_offset, MODULE_MAX_PATH_LENGTH);
	uv_fs_t req  = UV_FS_REQ_INIT();
	debuglog("[%p] start[%s]\n", uv_fs_get_data(&req), path);
	uv_fs_unlink(worker_thread_get_libuv_handle(), &req, path, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);

	return ret;
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

i32
wasm_get_time(i32 clock_id, i32 timespec_off)
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

#define SYS_EXIT_GROUP 231
i32
wasm_exit_group(i32 status)
{
	exit(status);
	return 0;
}

#define SYS_FCHOWN 93
i32
wasm_fchown(i32 file_descriptor, uint32_t owner, uint32_t group)
{
	int     d   = current_sandbox_get_file_descriptor(file_descriptor);
	uv_fs_t req = UV_FS_REQ_INIT();
	debuglog("[%p] start[%d,%d]\n", uv_fs_get_data(&req), file_descriptor, d);
	uv_fs_fchown(worker_thread_get_libuv_handle(), &req, d, owner, group, wasm_fs_callback);
	worker_thread_block_current_sandbox();

	int ret = uv_fs_get_result(&req);
	debuglog("[%p] end[%d]\n", uv_fs_get_data(&req), ret);
	uv_fs_req_cleanup(&req);

	return ret;
}

// networking syscalls
#define SYS_SOCKET  41
#define SYS_CONNECT 42
#define SYS_ACCEPT  43
#define SYS_BIND    49
#define SYS_LISTEN  50

static void
wasm_connection_callback(uv_stream_t *srv, int status)
{
	sandbox_t *s = srv->data;
	debuglog(" [%p]\n", s);
	s->return_value = status;
	worker_thread_wakeup_sandbox(s);
}

static void
wasm_connect_callback(uv_connect_t *req, int status)
{
	// TODO: how do we use the handle in uv_connect_t ??
	sandbox_t *s = req->data;
	debuglog(" [%p]\n", s);
	s->return_value = status;
	worker_thread_wakeup_sandbox(s);
}

i32
wasm_socket(i32 domain, i32 type, i32 protocol)
{
	struct sandbox *c   = current_sandbox_get();
	int             pfd = current_sandbox_initialize_io_handle(), file_descriptor = -1;
	// TODO: use domain, type and protocol in libuv?
	debuglog("[%p] => %d %d %d\n", c, domain, type, protocol);
	if (pfd < 0) return pfd;
	union uv_any_handle *h = current_sandbox_get_libuv_handle(pfd);

	if (type & SOCK_DGRAM) {
		uv_udp_t *uh = (uv_udp_t *)h;
		uh->data     = c;
		uv_udp_init(worker_thread_get_libuv_handle(), uh);
		debuglog(" udp init done!\n");
	} else if (type & SOCK_STREAM) {
		uv_tcp_t *uh = (uv_tcp_t *)h;
		uh->data     = c;
		uv_tcp_init(worker_thread_get_libuv_handle(), uh);
		debuglog(" tcp init done!\n");
	} else {
		assert(0); // not supported yet!
	}

	return pfd;
}

i32
wasm_connect(i32 sockfd, i32 sockaddr_offset, i32 addrlen)
{
	struct sandbox *c               = current_sandbox_get();
	int             file_descriptor = current_sandbox_get_file_descriptor(sockfd);
	debuglog("[%p] [%d, %d]\n", c, sockfd, file_descriptor);
	union uv_any_handle *h = current_sandbox_get_libuv_handle(sockfd);
	uv_handle_type       t = ((uv_handle_t *)h)->type;

	if (t == UV_TCP) {
		uv_connect_t req = { .data = c };
		debuglog("[%p] connect\n", c);
		int r = uv_tcp_connect(&req, (uv_tcp_t *)h, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen),
		                       wasm_connect_callback);
		worker_thread_block_current_sandbox();

		debuglog("[%p] %d\n", c, c->return_value);
		return c->return_value;
	} else if (t == UV_UDP) {
		debuglog(" UDP connect not implemented!\n");
		// TODO: this api is in the doc online but not in the libuv installed.. perhaps update??
		// return uv_udp_connect((uv_udp_t *)h, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen));
	}
	debuglog(" unsupported\n");
	assert(0);
	return -1;
}

i32
wasm_accept(i32 sockfd, i32 sockaddr_offset, i32 addrlen_offset)
{
	// what do we do with the sockaddr TODO: ????
	socklen_t *          addrlen        = worker_thread_get_memory_ptr_void(addrlen_offset, sizeof(socklen_t));
	struct sockaddr *    socket_address = worker_thread_get_memory_ptr_void(sockaddr_offset, *addrlen);
	union uv_any_handle *s              = current_sandbox_get_libuv_handle(sockfd);
	int                  cfd            = current_sandbox_initialize_io_handle();
	if (cfd < 0) return -1;
	struct sandbox *c = current_sandbox_get();
	debuglog("[%p] [%d, %d]\n", c, sockfd, current_sandbox_get_file_descriptor(sockfd));

	// assert so we can look into whether we need to implement UDP or others..
	assert(((uv_handle_t *)s)->type == UV_TCP);
	union uv_any_handle *h = current_sandbox_get_libuv_handle(cfd);
	uv_tcp_init(worker_thread_get_libuv_handle(), (uv_tcp_t *)h);
	debuglog("[%p] tcp init %d\n", c, cfd);
	int r = uv_accept((uv_stream_t *)s, (uv_stream_t *)h);
	if (r < 0) return r;
	// TODO: if accept fails, what do we do with the preopened handle?
	//	if (r < 0) current_sandbox_close_file_descriptor(cfd);
	// we've to also remove it from the runtime loop??

	int r2 = -1, f = -1;
	r2 = uv_fileno((uv_handle_t *)h, &f);
	if (r2 < 0 || f < 0) assert(0);
	current_sandbox_set_file_descriptor(cfd, f);
	debuglog("[%p] done[%d,%d]\n", c, cfd, f);

	return cfd;
}

i32
wasm_bind(i32 sockfd, i32 sockaddr_offset, i32 addrlen)
{
	struct sandbox *c               = current_sandbox_get();
	int             file_descriptor = current_sandbox_get_file_descriptor(sockfd);
	debuglog("[%p] [%d,%d]\n", c, sockfd, file_descriptor);
	union uv_any_handle *h = current_sandbox_get_libuv_handle(sockfd);
	uv_handle_type       t = ((uv_handle_t *)h)->type;

	if (t == UV_TCP) {
		debuglog("[%p] tcp\n", c);
		int r1 = uv_tcp_bind((uv_tcp_t *)h, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen),
		                     0 /* TODO: flags */);
		if (file_descriptor == SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC) {
			int r2 = -1, f = -1;
			r2 = uv_fileno((uv_handle_t *)h, &f);
			debuglog("[%p] [%d,%d]\n", c, f, file_descriptor);
			current_sandbox_set_file_descriptor(sockfd, f);
		}
		return r1;
	} else if (t == UV_UDP) {
		debuglog("[%p] udp\n", c);
		int r1 = uv_udp_bind((uv_udp_t *)h, worker_thread_get_memory_ptr_void(sockaddr_offset, addrlen),
		                     0 /* TODO: flags */);
		if (file_descriptor == SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC) {
			int r2 = -1, f = -1;
			r2 = uv_fileno((uv_handle_t *)h, &f);
			debuglog("[%p] [%d,%d]\n", c, f, file_descriptor);
			current_sandbox_set_file_descriptor(sockfd, f);
		}
		return r1;
	}
	debuglog("[%p] unimplemented\n", c);
	assert(0);
	return -1;
}

i32
wasm_listen(i32 sockfd, i32 backlog)
{
	struct sandbox *     c = current_sandbox_get();
	union uv_any_handle *h = current_sandbox_get_libuv_handle(sockfd);
	assert(c == (struct sandbox *)(((uv_tcp_t *)h)->data));
	debuglog("[%p] [%d,%d]\n", c, sockfd, current_sandbox_get_file_descriptor(sockfd));
	uv_handle_type t = ((uv_handle_t *)h)->type;

	// assert so we can look into whether we need to implement UDP or others..
	assert(t == UV_TCP);

	int r = uv_listen((uv_stream_t *)h, backlog, wasm_connection_callback);
	worker_thread_block_current_sandbox();

	debuglog("[%p] %d\n", c, c->return_value);
	return c->return_value;
}

#define SYS_SENDTO   44
#define SYS_RECVFROM 45

void
wasm_read_callback(uv_stream_t *s, ssize_t nread, const uv_buf_t *buffer)
{
	struct sandbox *c = s->data;

	debuglog("[%p] %ld %p\n", c, nread, buffer);
	if (nread < 0) c->return_value = -EIO;
	c->read_length = nread;
	debuglog("[%p] %ld\n", c, c->read_length);
	uv_read_stop(s);
	worker_thread_wakeup_sandbox(c);
}

void
wasm_write_callback(uv_write_t *req, int status)
{
	struct sandbox *c = req->data;
	c->return_value   = status;
	debuglog("[%p] %d\n", c, status);

	worker_thread_wakeup_sandbox(c);
}

void
wasm_udp_recv_callback(uv_udp_t *h, ssize_t nread, const uv_buf_t *buffer, const struct sockaddr *socket_address,
                       unsigned flags)
{
	struct sandbox *c = h->data;

	debuglog("[%p] %ld %p\n", c, nread, buffer);
	if (nread < 0) c->return_value = -EIO;
	c->read_length = nread;
	debuglog("[%p] %ld\n", c, c->read_length);
	uv_udp_recv_stop(h);
	worker_thread_wakeup_sandbox(c);
}

void
wasm_udp_send_callback(uv_udp_send_t *req, int status)
{
	struct sandbox *c = req->data;
	c->return_value   = status;
	debuglog("[%p] %d\n", c, status);

	worker_thread_wakeup_sandbox(c);
}

i32
wasm_sendto(i32 file_descriptor, i32 buff_offset, i32 len, i32 flags, i32 sockaddr_offset, i32 sockaddr_len)
{
	char *buffer = worker_thread_get_memory_ptr_void(buff_offset, len);
	// TODO: only support "send" api for now
	assert(sockaddr_len == 0);
	struct sandbox *     c = current_sandbox_get();
	union uv_any_handle *h = current_sandbox_get_libuv_handle(file_descriptor);
	uv_handle_type       t = ((uv_handle_t *)h)->type;
	debuglog("[%p] [%d,%d]\n", c, file_descriptor, current_sandbox_get_file_descriptor(file_descriptor));

	if (t == UV_TCP) {
		uv_write_t req = {
			.data = c,
		};
		uv_buf_t b = uv_buf_init(buffer, len);
		debuglog("[%p] tcp\n", c);
		int ret = uv_write(&req, (uv_stream_t *)h, &b, 1, wasm_write_callback);
		worker_thread_block_current_sandbox();

		debuglog("[%p] %d\n", c, c->return_value);
		return c->return_value;
	} else if (t == UV_UDP) {
		uv_udp_send_t req = {
			.data = c,
		};
		uv_buf_t b = uv_buf_init(buffer, len);
		debuglog("[%p] udp\n", c);
		// TODO: sockaddr!
		int r = uv_udp_send(&req, (uv_udp_t *)h, &b, 1, NULL, wasm_udp_send_callback);
		worker_thread_block_current_sandbox();

		debuglog("[%p] %d\n", c, c->return_value);
		return c->return_value;
	}
	debuglog("[%p] unimplemented\n", c);
	assert(0);
	return 0;
}

static inline void
wasm_alloc_callback(uv_handle_t *h, size_t suggested, uv_buf_t *buffer)
{
	struct sandbox *s = h->data;

	// just let it use what is passed from caller!
	buffer->base = s->read_buffer;
	buffer->len  = s->read_size;
}

i32
wasm_recvfrom(i32 file_descriptor, i32 buff_offset, i32 size, i32 flags, i32 sockaddr_offset, i32 socklen_offset)
{
	char *     buffer = worker_thread_get_memory_ptr_void(buff_offset, size);
	socklen_t *len    = worker_thread_get_memory_ptr_void(socklen_offset, sizeof(socklen_t));
	// TODO: only support "recv" api for now
	assert(*len == 0);
	struct sandbox *     c = current_sandbox_get();
	union uv_any_handle *h = current_sandbox_get_libuv_handle(file_descriptor);
	uv_handle_type       t = ((uv_handle_t *)h)->type;
	debuglog("[%p] [%d,%d]\n", c, file_descriptor, current_sandbox_get_file_descriptor(file_descriptor));

	// uv stream API are not simple wrappers on read/write..
	// and there will only be one system call pending..
	// so we keep the read buffer pointers in sandbox structure..
	// for use in the callbacks..
	c->read_buffer  = buffer;
	c->read_size    = size;
	c->read_length  = 0;
	c->return_value = 0;
	// TODO: what if stream read more than what "size" is here??

	if (t == UV_TCP) {
		((uv_stream_t *)h)->data = c;
		debuglog("[%p] tcp\n", c);
		int r = uv_read_start((uv_stream_t *)h, wasm_alloc_callback, wasm_read_callback);
		worker_thread_block_current_sandbox();
		debuglog("[%p] %d\n", c, c->return_value);
		if (c->return_value == -EIO) {
			// TODO: buffer errors??
		}
		if (r >= 0 && c->return_value == 0) { return c->read_length; }
		return -EIO;
	} else if (t == UV_UDP) {
		((uv_udp_t *)h)->data = c;
		debuglog("[%p] udp\n", c);
		int r = uv_udp_recv_start((uv_udp_t *)h, wasm_alloc_callback, wasm_udp_recv_callback);
		worker_thread_block_current_sandbox();
		debuglog("[%p] %d\n", c, c->return_value);
		if (c->return_value == -EIO) {
			// TODO: buffer errors??
		}
		if (r >= 0 && c->return_value == 0) { return c->read_length; }
		return -EIO;
	}
	debuglog("[%p] unimplemented\n", c);
	assert(0);
	return 0;
}

i32
inner_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
	i32 res;
	switch (n) {
	case SYS_READ:
		return wasm_read(a, b, c);
	case SYS_WRITE:
		return wasm_write(a, b, c);
	case SYS_OPEN:
		return wasm_open(a, b, c);
	case SYS_CLOSE:
		return wasm_close(a);
	case SYS_STAT:
		return wasm_stat(a, b);
	case SYS_FSTAT:
		return wasm_fstat(a, b);
	case SYS_LSTAT:
		return wasm_lstat(a, b);
	case SYS_LSEEK:
		return wasm_lseek(a, b, c);
	case SYS_MMAP:
		return wasm_mmap(a, b, c, d, e, f);
	case SYS_MUNMAP:
		return 0;
	case SYS_BRK:
		return 0;
	case SYS_RT_SIGACTION:
		return 0;
	case SYS_RT_SIGPROGMASK:
		return 0;
	case SYS_IOCTL:
		return wasm_ioctl(a, b, c);
	case SYS_READV:
		return wasm_readv(a, b, c);
	case SYS_WRITEV:
		return wasm_writev(a, b, c);
	case SYS_MADVISE:
		return 0;
	case SYS_GETPID:
		return wasm_getpid();
	case SYS_FCNTL:
		return wasm_fcntl(a, b, c);
	case SYS_FSYNC:
		return wasm_fsync(a);
	case SYS_UNLINK:
		return wasm_unlink(a);
	case SYS_GETCWD:
		return wasm_getcwd(a, b);
	case SYS_GETEUID:
		return wasm_geteuid();
	case SYS_SET_THREAD_AREA:
		return 0;
	case SYS_SET_TID_ADDRESS:
		return 0;
	case SYS_GET_TIME:
		return wasm_get_time(a, b);
	case SYS_EXIT_GROUP:
		return wasm_exit_group(a);
	case SYS_FCHOWN:
		return wasm_fchown(a, b, c);

	case SYS_SOCKET:
		return wasm_socket(a, b, c);
	case SYS_CONNECT:
		return wasm_connect(a, b, c);
	case SYS_ACCEPT:
		return wasm_accept(a, b, c);
	case SYS_BIND:
		return wasm_bind(a, b, c);
	case SYS_LISTEN:
		return wasm_listen(a, b);
	case SYS_SENDTO:
		return wasm_sendto(a, b, c, d, e, f);
	case SYS_RECVFROM:
		return wasm_recvfrom(a, b, c, d, e, f);
	}

	panic("syscall %d (%d, %d, %d, %d, %d, %d)\n", n, a, b, c, d, e, f);
	return 0;
}
