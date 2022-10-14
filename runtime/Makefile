ARCH := $(shell uname -m)
ifneq ($(ARCH),x86_64)
ifneq ($(ARCH),aarch64)
$(error Unsupported Architecture. Supports x86_64 and aarch64, found $(ARCH))
endif
endif

# Compiler Settings
CC=clang
CFLAGS=-std=c18 -pthread 

# We use several non-standard glibc / Linux features: 
# sched_getcpu, MAP_ANONYMOUS, acceess to 'gregs' in 'mcontext_t', REG_RIP, REG_RSP
CFLAGS+=-D_GNU_SOURCE

# Release Flags
CFLAGS+=-O3 -flto

# Debugging Flags
# CFLAGS+=-O0 -g3

# CFI Sanitizer
# CFLAGS+=-fvisibility=default -fsanitize=cfi

# Undefined Sanitizer - https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
# CFLAGS+=-fsanitize=undefined,float-divide-by-zero,unsigned-integer-overflow,implicit-conversion,local-bounds,nullability

# Clang SafeStack - https://clang.llvm.org/docs/SafeStack.html
# CFLAGS+=-fsanitize=safe-stack

# Memory Sanitizer - https://clang.llvm.org/docs/MemorySanitizer.html
# CFLAGS+=-fsanitize=memory -fno-omit-frame-pointer

# Address Sanitizer - "Fast Memory Error Detector" - https://clang.llvm.org/docs/AddressSanitizer.html
# CFLAGS+=-fsanitize=address -fno-omit-frame-pointer

BINARY_NAME=sledgert


# Feature Toggles
# CFLAGS += -DADMISSIONS_CONTROL

# Debugging Flags

# Enables logs of WASI syscalls
# CFLAGS += -DLOG_WASI

# Sandbox writes to stdout and stderr are both written to the client socket
# However, when set, stderr is also written to the host stderr to assist with debugging
# CFLAGS += -DLOG_SANDBOX_STDERR

# Strips out calls to assert() and disables debuglog
# CFLAGS += -DNDEBUG

# Redirects debuglogs to /runtime/bin/sledge.log 
# CFLAGS += -DLOG_TO_FILE

# Various Informational Logs for Debugging
# CFLAGS += -DLOG_ADMISSIONS_CONTROL
# CFLAGS += -DLOG_CONTEXT_SWITCHES
# CFLAGS += -DLOG_HTTP_PARSER
# CFLAGS += -DLOG_TENANT_LOADING
# CFLAGS += -DLOG_PREEMPTION
# CFLAGS += -DLOG_SANDBOX_ALLOCATION
# CFLAGS += -DLOG_UNSUPPORTED_WASI

# Stores and logs extended signal information for each worker
# CFLAGS += -DLOG_SOFTWARE_INTERRUPT_COUNTS

# This adds an array of sandbox states to all sandbox structs and appends states at each transition
# The history trucates when the number of elements equal SANDBOX_STATE_HISTORY_CAPACITY 
# CFLAGS += -DLOG_STATE_CHANGES

# This dumps per module *.csv files containing the cycle a sandbox has been in RUNNING when each
# page is allocated. This helps understand the relationship to memory allocation and execution time.
# CFLAGS += -DLOG_SANDBOX_MEMORY_PROFILE

# This flag enables runtime-level metrics from procfs
# CFLAGS += -DPROC_STAT_METRICS

# This flag enables HTTP-level counters of incoming requests and outgoing responses, broken out by status code
# family, such as 2XX, 4XX, 5XX.
# To log, run `call http_total_log()` while in GDB
# CFLAGS += -DHTTP_TOTAL_COUNTERS

# This flag enables per-route counters of incoming requests and outgoing responses, broken out by status code
# family, such as 2XX, 4XX, 5XX.
# CFLAGS += -DHTTP_ROUTE_TOTAL_COUNTERS

# This flag enables per-route latency perf-logs. 
# This has a perf impact due to the use of locks.
# This flag has a dependency on the flag HTTP_ROUTE_TOTAL_COUNTERS
# CFLAGS += -DROUTE_LATENCY

# This flag tracks the total number of sandboxes in the various states
# It is useful to debug if sandboxes are "getting caught" in a particular state
 CFLAGS += -DSANDBOX_STATE_TOTALS

# This flag enables an per-worker atomic count of sandbox's local runqueue count in thread local storage
# Useful to debug if sandboxes are "getting caught" or "leaking" while in a local runqueue
# CFLAGS += -DLOG_LOCAL_RUNQUEUE

# System Configuration Flags

# Sets a flag equal to the processor architecture
CFLAGS += -D${ARCH}

# Preprocessor

# SLEdge serverless functions are *.so shared-libraries. The runtime thus requires the dynamic linker
# to load these functions at runtime. These *.so shared-libraries also depend on specific symbols from 
# the runtime to execute. The export-dynamic Linker flag adds all globals to the dynamic symbol table, 
# allowing the libraries acess to such symbols. The libm math library is used several places, including
# in backing functions that implement the WebAssembly instruction set.
LDFLAGS += -Wl,--export-dynamic -ldl -lm

# Our third-party dependencies build into a single dist directory to simplify configuration here.
LDFLAGS += -Lthirdparty/dist/lib/
INCLUDES += -Iinclude/ -Ithirdparty/dist/include/ -I../libsledge/include/

# CFILES
CFILES += src/*.c
CFILES += src/arch/${ARCH}/*.c
CFILES += src/libc/*.c
CFILES += thirdparty/dist/lib/http_parser.o

# Configuring Jasmine
JSMNCFLAGS += -DJSMN_STATIC
JSMNCFLAGS += -DJSMN_STRICT

# Force sledgert to rebuild when header files change
# This is a bit fragile, as it does not recurse subdirectories when detecting last changed times
HEADER_DEPS = thirdparty/dist/include/*.h include/*.h include/arch/x86_64/*.h include/arch/aarch64/*.h

.PHONY: all
all: thirdparty runtime

.PHONY: clean
clean: thirdparty.clean runtime.clean

# sledgert Rules
bin/${BINARY_NAME}: ${HEADER_DEPS} ${CFILES}
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${INCLUDES} ${CFLAGS} ${LDFLAGS} ${JSMNCFLAGS} -L/usr/lib/ ${CFILES} -o bin/${BINARY_NAME}

.PHONY: runtime
runtime: bin/${BINARY_NAME}

.PHONY: runtime.clean
runtime.clean:
	@rm -f bin/${BINARY_NAME}

# Thirdparty Dependency Rules
thirdparty/dist/lib/http_parser.o: thirdparty
thirdparty/dist/include/*.h: thirdparty

.PHONY: thirdparty
thirdparty:
	@echo "Compiling thirdparty"
	@make --no-print-directory -C thirdparty build

.PHONY: thirdparty.clean
thirdparty.clean:
	@make --no-print-directory -C thirdparty clean
