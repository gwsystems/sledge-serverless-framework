ARCH := $(shell uname -m)
ifneq ($(ARCH),x86_64)
ifneq ($(ARCH),aarch64)
$(error Unsupported Architecture. Supports x86_64 and aarch64, found $(ARCH))
endif
endif

# Compiler Settings
CC=clang
CC_OPTIONS = -O3 -flto -g -pthread -D_GNU_SOURCE -std=c18

# CC_OPTIONS for Debugging
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE

# CFI Sanitizer
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE -flto -fvisibility=default -fsanitize=cfi 

# Undefined Sanitizer
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE -fsanitize=undefined,float-divide-by-zero,unsigned-integer-overflow,implicit-conversion,local-bounds 

# Clang SafeStack
# https://clang.llvm.org/docs/SafeStack.html
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE -fsanitize=safe-stack

# Memory Sanitizer 
# https://clang.llvm.org/docs/MemorySanitizer.html
# https://github.com/google/sanitizers/wiki/MemorySanitizer
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE -fsanitize=memory -fno-omit-frame-pointer

# Address Sanitizer
# https://clang.llvm.org/docs/AddressSanitizer.html
# CC_OPTIONS = -O0 -g -pthread -D_GNU_SOURCE -fsanitize=address -fno-omit-frame-pointer 

BINARY_NAME=sledgert


# Feature Toggles
# CFLAGS += -DADMISSIONS_CONTROL

# Debugging Flags

# Enables logs of WASI syscalls
# CFLAGS += -DLOG_WASI

# Writes sandbox stderr to host stderr
# CFLAGS += -DLOG_SANDBOX_STDERR

# Strips out calls to assert() and disables debuglog
# CFLAGS += -DNDEBUG

# Redirects debuglogs to /runtime/bin/sledge.log 
# CFLAGS += -DLOG_TO_FILE

# Various Informational Logs for Debugging
# CFLAGS += -DLOG_HTTP_PARSER
# CFLAGS += -DLOG_STATE_CHANGES
# CFLAGS += -DLOG_LOCK_OVERHEAD
# CFLAGS += -DLOG_CONTEXT_SWITCHES
# CFLAGS += -DLOG_ADMISSIONS_CONTROL
# CFLAGS += -DLOG_REQUEST_ALLOCATION
# CFLAGS += -DLOG_PREEMPTION
# CFLAGS += -DLOG_MODULE_LOADING

# This dumps per module *.csv files containing the cycle a sandbox has been in RUNNING when each
# page is allocated. This helps understand the relationship to memory allocation and execution time.
# CFLAGS += -DLOG_SANDBOX_MEMORY_PROFILE

# This flag dumps totals of incoming requests and outgoing responses, broken out by status code
# family, such as 2XX, 4XX, 5XX. It is useful to debug clients hanging waiting for a response.
# To log, run `call http_total_log()` while in GDB
# CFLAGS += -DLOG_TOTAL_REQS_RESPS

# This flag logs the total number of sandboxes in the various states
# It is useful to debug if sandboxes are "getting caught" in a particular state
# To log, run `call runtime_log_sandbox_states()` while in GDB
# CFLAGS += -DLOG_SANDBOX_COUNT

# This flag enables an per-worker atomic count of sandbox's local runqueue count in thread local storage
# Useful to debug if sandboxes are "getting caught" or "leaking" while in a local runqueue
# CFLAGS += -DLOG_LOCAL_RUNQUEUE

# System Configuration Flags

# Sets a flag equal to the processor architecture
CFLAGS += -D${ARCH}

<<<<<<< HEAD

=======
>>>>>>> chore: cleanup build tooling
# Preprocessor

# SLEdge serverless functions are *.so shared-libraries. The runtime thus requires the dynamic linker
# to load these functions at runtime. These *.so shared-libraries also depend on specific symbols from 
# the runtime to execute. The export-dynamic Linker flag adds all globals to the dynamic symbol table, 
# allowing the libraries acess to such symbols. The libm math library is used several places, including
# in backing functions that implement the WebAssembly instruction set.
LDFLAGS += -Wl,--export-dynamic -ldl -lm

# Our third-party dependencies build into a single dist directory to simplify configuration here.
LDFLAGS += -Lthirdparty/dist/lib/
INCLUDES += -Iinclude/ -Ithirdparty/dist/include/

# CFILES
CFILES += src/*.c
CFILES += src/arch/${ARCH}/*.c
CFILES += src/libc/*.c
CFILES += src/memory/common.c
CFILES += src/memory/64bit_nix.c
CFILES += thirdparty/dist/lib/http_parser.o

# Configuring Jasmine
JSMNCFLAGS += -DJSMN_STATIC
JSMNCFLAGS += -DJSMN_STRICT

.PHONY: all
all: runtime

bin/sledgert:
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${CC_OPTIONS} ${INCLUDES} ${CFLAGS} ${LDFLAGS} ${CFILES} ${JSMNCFLAGS} -L/usr/lib/ -o bin/sledgert

.PHONY: runtime
runtime: bin/sledgert

.PHONY: thirdparty
thirdparty:
	@echo "Compiling thirdparty"
	@make --no-print-directory -C thirdparty build

.PHONY: clean
clean:
	@rm -f core
	@echo "Cleaning up runtime"
	@rm -f bin/${BINARY_NAME}

.PHONY: distclean
distclean: clean
	@make --no-print-directory -C thirdparty clean

.PHONY: submodules
submodules:
	@git submodule update --init --recursive

.PHONY: init
init: submodules clean thirdparty runtime
