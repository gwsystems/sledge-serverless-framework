#include <stdint.h>
#include "sledge_abi.h"

// TODO: Validate uint32_t as return value;

uint32_t
wasi_snapshot_preview1_args_get(__wasi_size_t argv_retoffset, __wasi_size_t argv_buf_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_args_get(argv_buf_retoffset, argv_buf_retoffset);
}

uint32_t
wasi_snapshot_preview1_args_sizes_get(__wasi_size_t argc_retoffset, __wasi_size_t argv_buf_len_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_args_sizes_get(argc_retoffset, argv_buf_len_retoffset);
};

uint32_t
wasi_snapshot_preview1_clock_res_get(__wasi_clockid_t id, __wasi_size_t resolution_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_clock_res_get(id, resolution_retoffset);
}

uint32_t
wasi_snapshot_preview1_clock_time_get(__wasi_clockid_t clock_id, __wasi_timestamp_t precision,
                                      __wasi_size_t time_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_clock_time_get(clock_id, precision, time_retoffset);
}

uint32_t
wasi_snapshot_preview1_environ_get(__wasi_size_t env_retoffset, __wasi_size_t env_buf_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_environ_get(env_retoffset, env_buf_retoffset);
}

uint32_t
wasi_snapshot_preview1_environ_sizes_get(__wasi_size_t envc_retoffset, __wasi_size_t env_buf_len_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_environ_sizes_get(envc_retoffset, env_buf_len_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_advise(__wasi_fd_t fd, __wasi_filesize_t file_offset, __wasi_filesize_t len,
                                 uint32_t advice_extended)
{
	return sledge_abi__wasi_snapshot_preview1_fd_advise(fd, file_offset, len, advice_extended);
}

uint32_t
wasi_snapshot_preview1_fd_allocate(__wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len)
{
	return sledge_abi__wasi_snapshot_preview1_fd_allocate(fd, offset, len);
}

uint32_t
wasi_snapshot_preview1_fd_close(__wasi_fd_t fd)
{
	return sledge_abi__wasi_snapshot_preview1_fd_close(fd);
}

uint32_t
wasi_snapshot_preview1_fd_datasync(__wasi_fd_t fd)
{
	return sledge_abi__wasi_snapshot_preview1_fd_datasync(fd);
}

uint32_t
wasi_snapshot_preview1_fd_fdstat_get(__wasi_fd_t fd, __wasi_size_t fdstat_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_fdstat_get(fd, fdstat_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_fdstat_set_flags(__wasi_fd_t fd, uint32_t fdflags_extended)
{
	return sledge_abi__wasi_snapshot_preview1_fd_fdstat_set_flags(fd, fdflags_extended);
}

uint32_t
wasi_snapshot_preview1_fd_fdstat_set_rights(__wasi_fd_t fd, __wasi_rights_t fs_rights_base,
                                            __wasi_rights_t fs_rights_inheriting)
{
	return sledge_abi__wasi_snapshot_preview1_fd_fdstat_set_rights(fd, fs_rights_base, fs_rights_inheriting);
}

uint32_t
wasi_snapshot_preview1_fd_filestat_get(__wasi_fd_t fd, __wasi_size_t filestat_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_filestat_get(fd, filestat_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_filestat_set_size(__wasi_fd_t fd, __wasi_filesize_t size)
{
	return sledge_abi__wasi_snapshot_preview1_fd_filestat_set_size(fd, size);
}

uint32_t
wasi_snapshot_preview1_fd_filestat_set_times(__wasi_fd_t fd, __wasi_timestamp_t atim, __wasi_timestamp_t mtim,
                                             uint32_t fstflags_extended)
{
	return sledge_abi__wasi_snapshot_preview1_fd_filestat_set_times(fd, atim, mtim, fstflags_extended);
}

uint32_t
wasi_snapshot_preview1_fd_pread(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                __wasi_filesize_t offset, __wasi_size_t nread_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_pread(fd, iovs_baseoffset, iovs_len, offset, nread_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_prestat_get(__wasi_fd_t fd, __wasi_size_t prestat_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_prestat_get(fd, prestat_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_prestat_dir_name(__wasi_fd_t fd, __wasi_size_t dirname_retoffset, __wasi_size_t dirname_len)
{
	return sledge_abi__wasi_snapshot_preview1_fd_prestat_dir_name(fd, dirname_retoffset, dirname_len);
}

uint32_t
wasi_snapshot_preview1_fd_pwrite(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                 __wasi_filesize_t file_offset, __wasi_size_t nwritten_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_pwrite(fd, iovs_baseoffset, iovs_len, file_offset,
	                                                    nwritten_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_read(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                               __wasi_size_t nread_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_read(fd, iovs_baseoffset, iovs_len, nread_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_readdir(__wasi_fd_t fd, __wasi_size_t buf_baseoffset, __wasi_size_t buf_len,
                                  __wasi_dircookie_t cookie, __wasi_size_t nread_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_readdir(fd, buf_baseoffset, buf_len, cookie, nread_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_renumber(__wasi_fd_t fd, __wasi_fd_t to)
{
	return sledge_abi__wasi_snapshot_preview1_fd_renumber(fd, to);
}

uint32_t
wasi_snapshot_preview1_fd_seek(__wasi_fd_t fd, __wasi_filedelta_t file_offset, uint32_t whence_extended,
                               __wasi_size_t newoffset_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_seek(fd, file_offset, whence_extended, newoffset_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_sync(__wasi_fd_t fd)
{
	return sledge_abi__wasi_snapshot_preview1_fd_sync(fd);
}

uint32_t
wasi_snapshot_preview1_fd_tell(__wasi_fd_t fd, __wasi_size_t fileoffset_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_tell(fd, fileoffset_retoffset);
}

uint32_t
wasi_snapshot_preview1_fd_write(__wasi_fd_t fd, __wasi_size_t iovs_baseoffset, __wasi_size_t iovs_len,
                                __wasi_size_t nwritten_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_fd_write(fd, iovs_baseoffset, iovs_len, nwritten_retoffset);
}

uint32_t
wasi_snapshot_preview1_path_create_directory(__wasi_fd_t fd, __wasi_size_t path_baseoffset, __wasi_size_t path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_create_directory(fd, path_baseoffset, path_len);
}

uint32_t
wasi_snapshot_preview1_path_filestat_get(__wasi_fd_t fd, __wasi_lookupflags_t flags, __wasi_size_t path_baseoffset,
                                         __wasi_size_t path_len, __wasi_size_t filestat_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_path_filestat_get(fd, flags, path_baseoffset, path_len,
	                                                            filestat_retoffset);
}

uint32_t
wasi_snapshot_preview1_path_filestat_set_times(__wasi_fd_t fd, __wasi_lookupflags_t flags,
                                               __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                               __wasi_timestamp_t atim, __wasi_timestamp_t mtim,
                                               uint32_t fstflags_extended)
{
	return sledge_abi__wasi_snapshot_preview1_path_filestat_set_times(fd, flags, path_baseoffset, path_len, atim,
	                                                                  mtim, fstflags_extended);
}

uint32_t
wasi_snapshot_preview1_path_link(__wasi_fd_t old_fd, __wasi_lookupflags_t old_flags, __wasi_size_t old_path_baseoffset,
                                 __wasi_size_t old_path_len, __wasi_fd_t new_fd, __wasi_size_t new_path_baseoffset,
                                 __wasi_size_t new_path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_link(old_fd, old_flags, old_path_baseoffset, old_path_len,
	                                                    new_fd, new_path_baseoffset, new_path_len);
}

uint32_t
wasi_snapshot_preview1_path_open(__wasi_fd_t dirfd, __wasi_lookupflags_t lookupflags, __wasi_size_t path_baseoffset,
                                 __wasi_size_t path_len, uint32_t oflags_extended, __wasi_rights_t fs_rights_base,
                                 __wasi_rights_t fs_rights_inheriting, uint32_t fdflags_extended,
                                 __wasi_size_t fd_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_path_open(dirfd, lookupflags, path_baseoffset, path_len,
	                                                    oflags_extended, fs_rights_base, fs_rights_inheriting,
	                                                    fdflags_extended, fd_retoffset);
}

uint32_t
wasi_snapshot_preview1_path_readlink(__wasi_fd_t fd, __wasi_size_t path_baseoffset, __wasi_size_t path_len,
                                     __wasi_size_t buf_baseretoffset, __wasi_size_t buf_len,
                                     __wasi_size_t nread_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_path_readlink(fd, path_baseoffset, path_len, buf_baseretoffset,
	                                                        buf_len, nread_retoffset);
}

uint32_t
wasi_snapshot_preview1_path_remove_directory(__wasi_fd_t fd, __wasi_size_t path_baseoffset, __wasi_size_t path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_remove_directory(fd, path_baseoffset, path_len);
}

uint32_t
wasi_snapshot_preview1_path_rename(__wasi_fd_t fd, __wasi_size_t old_path_baseoffset, __wasi_size_t old_path_len,
                                   __wasi_fd_t new_fd, __wasi_size_t new_path_baseoffset, __wasi_size_t new_path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_rename(fd, old_path_baseoffset, old_path_len, new_fd,
	                                                      new_path_baseoffset, new_path_len);
}

uint32_t
wasi_snapshot_preview1_path_symlink(__wasi_size_t old_path_baseoffset, __wasi_size_t old_path_len, __wasi_fd_t fd,
                                    __wasi_size_t new_path_baseoffset, __wasi_size_t new_path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_symlink(old_path_baseoffset, old_path_len, fd,
	                                                       new_path_baseoffset, new_path_len);
}

uint32_t
wasi_snapshot_preview1_path_unlink_file(__wasi_fd_t fd, __wasi_size_t path_baseoffset, __wasi_size_t path_len)
{
	return sledge_abi__wasi_snapshot_preview1_path_unlink_file(fd, path_baseoffset, path_len);
}

uint32_t
wasi_snapshot_preview1_poll_oneoff(__wasi_size_t in_baseoffset, __wasi_size_t out_baseoffset,
                                   __wasi_size_t nsubscriptions, __wasi_size_t nevents_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_poll_oneoff(in_baseoffset, out_baseoffset, nsubscriptions,
	                                                      nevents_retoffset);
}

void
wasi_snapshot_preview1_proc_exit(__wasi_exitcode_t exitcode)
{
	sledge_abi__wasi_snapshot_preview1_proc_exit(exitcode);
}

uint32_t
wasi_snapshot_preview1_proc_raise(uint32_t sig_extended)
{
	return sledge_abi__wasi_snapshot_preview1_proc_raise(sig_extended);
}

uint32_t
wasi_snapshot_preview1_random_get(__wasi_size_t buf_baseretoffset, __wasi_size_t buf_len)
{
	return sledge_abi__wasi_snapshot_preview1_random_get(buf_baseretoffset, buf_len);
}

uint32_t
wasi_snapshot_preview1_sched_yield(void)
{
	return sledge_abi__wasi_snapshot_preview1_sched_yield();
}

uint32_t
wasi_snapshot_preview1_sock_recv(__wasi_fd_t fd, __wasi_size_t ri_data_baseretoffset, __wasi_size_t ri_data_len,
                                 uint32_t ri_flags_extended, __wasi_size_t ri_data_nbytes_retoffset,
                                 __wasi_size_t message_nbytes_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_sock_recv(fd, ri_data_baseretoffset, ri_data_len, ri_flags_extended,
	                                                    ri_data_nbytes_retoffset, message_nbytes_retoffset);
}

uint32_t
wasi_snapshot_preview1_sock_send(__wasi_fd_t fd, __wasi_size_t si_data_baseoffset, __wasi_size_t si_data_len,
                                 uint32_t si_flags_extended, __wasi_size_t nbytes_retoffset)
{
	return sledge_abi__wasi_snapshot_preview1_sock_send(fd, si_data_baseoffset, si_data_len, si_flags_extended,
	                                                    nbytes_retoffset);
}

uint32_t
wasi_snapshot_preview1_sock_shutdown(__wasi_fd_t fd, uint32_t how)
{
	return sledge_abi__wasi_snapshot_preview1_sock_shutdown(fd, how);
}
