#ifndef SFRT_TYPES_H
#define SFRT_TYPES_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <printf.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/uio.h>

#define EXPORT __attribute__ ((visibility ("default")))
#define IMPORT __attribute__ ((visibility ("default")))

#define INLINE __attribute__((always_inline))
#define WEAK __attribute__((weak))

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 32
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE (1<<12)
#endif

#define CACHE_ALIGNED __attribute__((aligned(CACHELINE_SIZE)))
#define PAGE_ALIGNED __attribute__((aligned(PAGE_SIZE)))

/* For this family of macros, do NOT pass zero as the pow2 */
#define round_to_pow2(x, pow2) (((unsigned long)(x)) & (~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x) + (pow2)-1, (pow2)))

#define round_to_page(x) round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x) round_up_to_pow2(x, PAGE_SIZE)

// Type alias's so I don't have to write uint32_t a million times
typedef signed char i8;
typedef unsigned char u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

// FIXME: per-module configuration?
#define WASM_PAGE_SIZE (1024 * 64) //64KB
#define WASM_START_PAGES (1<<8) //16MB
#define WASM_MAX_PAGES (1<<15) //4GB

#define WASM_STACK_SIZE (1<<14) // 16KB.
#define SBOX_MAX_MEM (1L<<32) // 4GB

// These are per module symbols and I'd need to dlsym for each module. instead just use global constants, see above macros.
// The code generator compiles in the starting number of wasm pages, and the maximum number of pages
// If we try and allocate more than max_pages, we should fault
//extern u32 starting_pages;
//extern u32 max_pages;

// The code generator also compiles in stubs that populate the linear memory and function table
void populate_memory(void);
void populate_table(void);

// memory/* also provides the table access functions
// TODO: Change this to use a compiled in size
#define INDIRECT_TABLE_SIZE 1024

struct indirect_table_entry {
	u32 type_id;
	void *func_pointer;
};

extern __thread struct indirect_table_entry *module_indirect_table;

// for sandbox linear memory isolation
extern __thread void *sandbox_lmbase;
extern __thread u32 sandbox_lmbound;
extern i32 logfd;

// functions in the module to lookup and call per sandbox.
typedef i32 (*mod_main_fn_t)(i32 a, i32 b);
typedef void (*mod_glb_fn_t)(void);
typedef void (*mod_mem_fn_t)(void);
typedef void (*mod_tbl_fn_t)(void);
typedef void (*mod_init_libc_fn_t)(i32, i32);

typedef enum {
	MOD_ARG_MODPATH = 0,
	MOD_ARG_MODPORT,
	MOD_ARG_MODNAME,
	MOD_ARG_MODNARGS,
	MOD_ARG_MAX,
} mod_argindex_t;

#define MOD_MAIN_FN "wasmf_main"
#define MOD_GLB_FN  "populate_globals"
#define MOD_MEM_FN  "populate_memory"
#define MOD_TBL_FN  "populate_table"
#define MOD_INIT_LIBC_FN "wasmf___init_libc"

#define MOD_MAX_ARGS   16
#define MOD_ARG_MAX_SZ 64
#define MOD_MAX        1024

#define MOD_NAME_MAX 32
#define MOD_PATH_MAX 256

#define JSON_ELE_MAX 16

// FIXME: some naive work-stealing here..
#define SBOX_PULL_MAX 16

#define SBOX_MAX_OPEN 32
#define SBOX_PREOPEN_MAGIC (707707707) // reads lol lol lol upside down

#define SOFTINT_TIMER_START_USEC (10*1000) //start timers 10 ms from now.
#define SOFTINT_TIMER_PERIOD_USEC (1000*100) // 100ms timer..

#ifdef DEBUG
#ifdef NOSTDIO
#define debuglog(fmt,...) dprintf(logfd, "(%d,%lu) %s: " fmt, sched_getcpu(), pthread_self(), __func__, ## __VA_ARGS__)
#else
#define debuglog(fmt,...) printf("(%d,%lu) %s: " fmt, sched_getcpu(), pthread_self(), __func__, ## __VA_ARGS__)
#endif
#else
#define debuglog(fmt,...)
#endif

#define GLB_STDOUT "/dev/null"
#define GLB_STDERR "/dev/null"
#define GLB_STDIN  "/dev/zero"

#define LOGFILE "awesome.log"

#define RDWR_VEC_MAX 16

#define MOD_REQ_CORE 0 // core dedicated to check module requests..
#define SBOX_NCORES (NCORES > 1 ? NCORES - 1 : NCORES)  // number of sandboxing threads
#define SBOX_MAX_REQS (1<<19) //random!

#define SBOX_RESP_STRSZ 32

#define MOD_BACKLOG 10000
#define EPOLL_MAX 1024
#define MOD_REQ_RESP_DEFAULT (PAGE_SIZE)
#define QUIESCENSE_TIME (1<<20) //cycles!

#define HTTP_HEADERS_MAX     6
#define HTTP_HEADER_MAXSZ    32
#define HTTP_HEADERVAL_MAXSZ 64

#endif /* SFRT_TYPES_H */
