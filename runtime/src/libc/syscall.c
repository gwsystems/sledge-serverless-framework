#ifdef USE_SYSCALL

/* code from https://github.com/gwsystems/silverfish/blob/master/runtime/libc/libc_backing.c */
#include <runtime.h>
#include <sys/types.h>
#include <sys/socket.h>

// What should we tell the child program its UID and GID are?
#define UID 0xFF
#define GID 0xFE

// Elf auxilary vector values (see google for what those are)
#define AT_NULL		0
#define AT_IGNORE	1
#define AT_EXECFD	2
#define AT_PHDR		3
#define AT_PHENT	4
#define AT_PHNUM	5
#define AT_PAGESZ	6
#define AT_BASE		7
#define AT_FLAGS	8
#define AT_ENTRY	9
#define AT_NOTELF	10
#define AT_UID		11
#define AT_EUID		12
#define AT_GID		13
#define AT_EGID		14
#define AT_CLKTCK	17
#define	AT_SECURE	23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM	25

// offset = a WASM ptr to memory the runtime can use
void
stub_init(char *program_name, i32 offset, mod_init_libc_fn_t libcfn)
{
	printf("Don't think we should reinit libc! so ignore for now!\n");
}

// Emulated syscall implementations

// We define our own syscall numbers, because WASM uses x86_64 values even on systems that are not x86_64
#define SYS_READ 0
u32
wasm_read(i32 filedes, i32 buf_offset, i32 nbyte)
{
	char* buf = get_memory_ptr_void(buf_offset, nbyte);
	i32 res = (i32) read(filedes, buf, nbyte);

	if (res == -1) return -errno;
	
	return res;
}

#define SYS_WRITE 1
i32
wasm_write(i32 fd, i32 buf_offset, i32 buf_size)
{
	char* buf = get_memory_ptr_void(buf_offset, buf_size);
	i32 res = (i32) write(fd, buf, buf_size);

	if (res == -1) return -errno;

	return res;
}

#define WO_RDONLY  00
#define WO_WRONLY  01
#define WO_RDWR    02
#define WO_CREAT        0100
#define WO_EXCL         0200
#define WO_NOCTTY       0400
#define WO_TRUNC       01000
#define WO_APPEND      02000
#define WO_NONBLOCK    04000
#define WO_DSYNC      010000
#define WO_SYNC     04010000
#define WO_RSYNC    04010000
#define WO_DIRECTORY 0200000
#define WO_NOFOLLOW  0400000
#define WO_CLOEXEC  02000000


#define SYS_OPEN 2
i32
wasm_open(i32 path_off, i32 flags, i32 mode)
{
	char* path = get_memory_string(path_off);

	i32 modified_flags = 0;

	if (flags & WO_RDONLY) {
		modified_flags |= O_RDONLY;
		flags ^= WO_RDONLY;
	}

	if (flags & WO_WRONLY) {
		modified_flags |= O_WRONLY;
		flags ^= WO_WRONLY;
	}

	if (flags & WO_RDWR) {
		modified_flags |= O_RDWR;
		flags ^= WO_RDWR;
	}

	if (flags & WO_APPEND) {
		modified_flags |= O_APPEND;
		flags ^= WO_APPEND;
	}

	if (flags & WO_CREAT) {
		modified_flags |= O_CREAT;
		flags ^= WO_CREAT;
	}

	if (flags & WO_EXCL) {
		modified_flags |= O_EXCL;
		flags ^= WO_EXCL;
	}

	i32 res = (i32) open(path, modified_flags, mode);

	if (res == -1) return -errno;

	return res;

}

#define SYS_CLOSE 3
i32
wasm_close(i32 fd)
{
	i32 res = (i32) close(fd);

	if (res == -1) return -errno;

	return res;
}

// What the wasm stat structure looks like
struct wasm_stat {
	i64 st_dev;
	u64 st_ino;
	u32 st_nlink;

	u32 st_mode;
	u32 st_uid;
	u32 st_gid;
	u32 __pad0;
	u64 st_rdev;
	u64 st_size;
	i32 st_blksize;
	i64 st_blocks;

	struct { i32 tv_sec; i32 tv_nsec; } st_atim;
	struct { i32 tv_sec; i32 tv_nsec; } st_mtim;
	struct { i32 tv_sec; i32 tv_nsec; } st_ctim;
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
wasm_stat(u32 path_str_offset, i32 stat_offset)
{
	char *path = get_memory_string(path_str_offset);
	struct wasm_stat* stat_ptr = get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	i32 res = lstat(path, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat) {
		.st_dev = stat.st_dev,
			.st_ino = stat.st_ino,
			.st_nlink = stat.st_nlink,
			.st_mode = stat.st_mode,
			.st_uid = stat.st_uid,
			.st_gid = stat.st_gid,
			.st_rdev = stat.st_rdev,
			.st_size = stat.st_size,
			.st_blksize = stat.st_blksize,
			.st_blocks = stat.st_blocks,
	};
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}

#define SYS_FSTAT 5
i32
wasm_fstat(i32 filedes, i32 stat_offset)
{
	struct wasm_stat* stat_ptr = get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	i32 res = fstat(filedes, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat) {
		.st_dev = stat.st_dev,
			.st_ino = stat.st_ino,
			.st_nlink = stat.st_nlink,
			.st_mode = stat.st_mode,
			.st_uid = stat.st_uid,
			.st_gid = stat.st_gid,
			.st_rdev = stat.st_rdev,
			.st_size = stat.st_size,
			.st_blksize = stat.st_blksize,
			.st_blocks = stat.st_blocks,
	};
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}

#define SYS_LSTAT 6
i32
wasm_lstat(i32 path_str_offset, i32 stat_offset)
{
	char *path = get_memory_string(path_str_offset);
	struct wasm_stat* stat_ptr = get_memory_ptr_void(stat_offset, sizeof(struct wasm_stat));

	struct stat stat;
	i32 res = lstat(path, &stat);
	if (res == -1) return -errno;

	*stat_ptr = (struct wasm_stat) {
		.st_dev = stat.st_dev,
			.st_ino = stat.st_ino,
			.st_nlink = stat.st_nlink,
			.st_mode = stat.st_mode,
			.st_uid = stat.st_uid,
			.st_gid = stat.st_gid,
			.st_rdev = stat.st_rdev,
			.st_size = stat.st_size,
			.st_blksize = stat.st_blksize,
			.st_blocks = stat.st_blocks,
	};
#ifdef __APPLE__
	stat_ptr->st_atim.tv_sec = stat.st_atimespec.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atimespec.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtimespec.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtimespec.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctimespec.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctimespec.tv_nsec;
#else
	stat_ptr->st_atim.tv_sec = stat.st_atim.tv_sec;
	stat_ptr->st_atim.tv_nsec = stat.st_atim.tv_nsec;

	stat_ptr->st_mtim.tv_sec = stat.st_mtim.tv_sec;
	stat_ptr->st_mtim.tv_nsec = stat.st_mtim.tv_nsec;

	stat_ptr->st_ctim.tv_sec = stat.st_ctim.tv_sec;
	stat_ptr->st_ctim.tv_nsec = stat.st_ctim.tv_nsec;
#endif

	return res;
}


#define SYS_LSEEK 8
i32
wasm_lseek(i32 filedes, i32 file_offset, i32 whence)
{
	i32 res = (i32) lseek(filedes, file_offset, whence);

	if (res == -1) return -errno;

	return res;
}

#define SYS_MMAP 9
u32
wasm_mmap(i32 addr, i32 len, i32 prot, i32 flags, i32 fd, i32 offset)
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

	i32 result = sandbox_lmbound;
	for (int i = 0; i < len / WASM_PAGE_SIZE; i++) {
		expand_memory();
	}

	return result;
}

#define SYS_MUNMAP 11

#define SYS_BRK 12

#define SYS_RT_SIGACTION 13

#define SYS_RT_SIGPROGMASK 14

#define SYS_IOCTL 16
i32
wasm_ioctl(i32 fd, i32 request, i32 data_offet)
{
	// musl libc does some ioctls to stdout, so just allow these to silently go through
	// FIXME: The above is idiotic
	return 0;
}

#define SYS_READV 19
struct wasm_iovec {
	i32 base_offset;
	i32 len;
};

i32
wasm_readv(i32 fd, i32 iov_offset, i32 iovcnt)
{
	i32 read = 0;
	struct wasm_iovec *iov = get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));
	for (int i = 0; i < iovcnt; i++) {
		read += wasm_read(fd, iov[i].base_offset, iov[i].len);
	}

	return read;
}

#define SYS_WRITEV 20
i32
wasm_writev(i32 fd, i32 iov_offset, i32 iovcnt)
{
	struct wasm_iovec *iov = get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

	// If we aren't on MUSL, pass writev to printf if possible
#if defined(__APPLE__) || defined(__GLIBC__)
	if (fd == 1) {
		int sum = 0;
		for (int i = 0; i < iovcnt; i++) {
			i32 len = iov[i].len;
			void* ptr = get_memory_ptr_void(iov[i].base_offset, len);

			printf("%.*s", len, ptr);
			sum += len;
		}
		return sum;
	}
#endif

	struct iovec vecs[iovcnt];
	for (int i = 0; i < iovcnt; i++) {
		i32 len = iov[i].len;
		void* ptr = get_memory_ptr_void(iov[i].base_offset, len);
		vecs[i] = (struct iovec) {ptr, len};
	}

	i32 res = (i32) writev(fd, vecs, iovcnt);
	if (res == -1) return -errno;

	return res;
}

#define SYS_MADVISE 28

#define SYS_GETPID 39
u32
wasm_getpid()
{
	return (u32) getpid();
}


#define WF_DUPFD  0
#define WF_GETFD  1
#define WF_SETFD  2
#define WF_GETFL  3
#define WF_SETFL  4

#define WF_SETOWN 8
#define WF_GETOWN 9
#define WF_SETSIG 10
#define WF_GETSIG 11

#define WF_GETLK 5
#define WF_SETLK 6
#define WF_SETLKW 7

#define SYS_FCNTL 72
u32
wasm_fcntl(u32 fd, u32 cmd, u32 arg_or_lock_ptr)
{
	switch (cmd) {
		case WF_SETFD:
			//            return fcntl(fd, F_SETFD, arg_or_lock_ptr);
			return 0;
		case WF_SETLK:
			return 0;
		default:
			assert(0);
	}

}

#define SYS_FSYNC 74
u32
wasm_fsync(u32 filedes)
{
	u32 res = fsync(filedes);
	if (res == -1) return -errno;

	return 0;
}

#define SYS_GETCWD 79
u32
wasm_getcwd(u32 buf_offset, u32 buf_size)
{
	char* buf = get_memory_ptr_void(buf_offset, buf_size);
	char* res = getcwd(buf, buf_size);

	if (!res) return 0;
	return buf_offset;
}

#define SYS_UNLINK 87
u32
wasm_unlink(u32 path_str_offset)
{
	char* str = get_memory_string(path_str_offset);
	u32 res = unlink(str);
	if (res == -1) return -errno;

	return 0;
}

#define SYS_GETEUID 107
u32
wasm_geteuid()
{
	return (u32) geteuid();
}

#define SYS_SET_THREAD_AREA 205

#define SYS_SET_TID_ADDRESS 218

#define SYS_GET_TIME 228
struct wasm_time_spec {
	u32 sec;
	u32 nanosec;
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

	struct wasm_time_spec* timespec = get_memory_ptr_void(timespec_off, sizeof(struct wasm_time_spec));

	struct timespec native_timespec = { 0, 0 };
	int res = clock_gettime(real_clock, &native_timespec);
	if (res == -1) return -errno;

	timespec->sec = native_timespec.tv_sec;
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
wasm_fchown(i32 fd, u32 owner, u32 group)
{
	return fchown(fd, owner, group);
}

// networking syscalls
#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_ACCEPT 43
#define SYS_BIND 49
#define SYS_LISTEN 50
i32
wasm_socket(i32 domain, i32 type, i32 protocol)
{
	return socket(domain, type, protocol);
}

i32
wasm_connect(i32 sockfd, i32 sockaddr_offset, i32 addrlen)
{
	return connect(sockfd, get_memory_ptr_void(sockaddr_offset, addrlen), addrlen);
}

i32
wasm_accept(i32 sockfd, i32 sockaddr_offset, i32 addrlen_offset)
{
	socklen_t *addrlen = get_memory_ptr_void(addrlen_offset, sizeof(socklen_t));

	return accept(sockfd, get_memory_ptr_void(sockaddr_offset, *addrlen), addrlen);
}

i32
wasm_bind(i32 sockfd, i32 sockaddr_offset, i32 addrlen)
{
	return bind(sockfd, get_memory_ptr_void(sockaddr_offset, addrlen), addrlen);
}

i32
wasm_listen(i32 sockfd, i32 backlog)
{
	return listen(sockfd, backlog);
}

#define SYS_SENDTO 44
#define SYS_RECVFROM 45

i32
wasm_sendto(i32 fd, i32 buff_offset, i32 len, i32 flags, i32 sockaddr_offset, i32 sockaddr_len)
{
	char *buf = get_memory_ptr_void(buff_offset, len);
	struct sockaddr *addr = sockaddr_len ? get_memory_ptr_void(sockaddr_offset, sockaddr_len) : NULL;

	return sendto(fd, buf, len, flags, addr, sockaddr_len);
}

i32
wasm_recvfrom(i32 fd, i32 buff_offset, i32 size, i32 flags, i32 sockaddr_offset, i32 socklen_offset)
{
	char *buf = get_memory_ptr_void(buff_offset, size);
	socklen_t *len = get_memory_ptr_void(socklen_offset, sizeof(socklen_t));
	struct sockaddr *addr = *len ? get_memory_ptr_void(sockaddr_offset, *len) : NULL;

	return recvfrom(fd, buf, size, flags, addr, addr ? len : NULL);
}

i32
inner_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
	i32 res;
	switch(n) {
		case SYS_READ: return wasm_read(a, b, c);
		case SYS_WRITE: return wasm_write(a, b, c);
		case SYS_OPEN: return wasm_open(a, b, c);
		case SYS_CLOSE: return wasm_close(a);
		case SYS_STAT: return wasm_stat(a, b);
		case SYS_FSTAT: return wasm_fstat(a, b);
		case SYS_LSTAT: return wasm_lstat(a, b);
		case SYS_LSEEK: return wasm_lseek(a, b, c);
		case SYS_MMAP: return wasm_mmap(a, b, c, d, e, f);
		case SYS_MUNMAP: return 0;
		case SYS_BRK: return 0;
		case SYS_RT_SIGACTION: return 0;
		case SYS_RT_SIGPROGMASK: return 0;
		case SYS_IOCTL: return wasm_ioctl(a, b, c);
		case SYS_READV: return wasm_readv(a, b, c);
		case SYS_WRITEV: return wasm_writev(a, b, c);
		case SYS_MADVISE: return 0;
		case SYS_GETPID: return wasm_getpid();
		case SYS_FCNTL: return wasm_fcntl(a, b, c);
		case SYS_FSYNC: return wasm_fsync(a);
		case SYS_UNLINK: return wasm_unlink(a);
		case SYS_GETCWD: return wasm_getcwd(a, b);
		case SYS_GETEUID: return wasm_geteuid();
		case SYS_SET_THREAD_AREA: return 0;
		case SYS_SET_TID_ADDRESS: return 0;
		case SYS_GET_TIME: return wasm_get_time(a, b);
		case SYS_EXIT_GROUP: return wasm_exit_group(a);
		case SYS_FCHOWN: return wasm_fchown(a, b, c);

		case SYS_SOCKET: return wasm_socket(a, b, c);
		case SYS_CONNECT: return wasm_connect(a, b, c);
		case SYS_ACCEPT: return wasm_accept(a, b, c);
		case SYS_BIND: return wasm_bind(a, b, c);
		case SYS_LISTEN: return wasm_listen(a, b);
		case SYS_SENDTO: return wasm_sendto(a, b, c, d, e, f);
		case SYS_RECVFROM: return wasm_recvfrom(a, b, c, d, e, f);
	}
	printf("syscall %d (%d, %d, %d, %d, %d, %d)\n", n, a, b, c, d, e, f);
	assert(0);

	return 0;
}

#endif
