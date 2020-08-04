ARCH := $(shell uname -m)
TOTAL_CORES := $(shell getconf _NPROCESSORS_CONF)
PAGE_SIZE=$(shell getconf PAGESIZE)

# Compiler Settings
CC=clang
CC_OPTIONS = -O3 -flto -g -pthread -D_GNU_SOURCE

BINARY_NAME=sledgert

NCORES = ${TOTAL_CORES} # Number of Cores. Options: {2...N or TOTAL_CORES}
USE_MEM = USE_MEM_VM # Options: {USE_MEM_GENERIC, USE_MEM_VM}

INCLUDES += -Iinclude/ -Ithirdparty/dist/include/

# CFILES
CFILES += src/*.c 
CFILES += src/arch/${ARCH}/*.c 
CFILES += src/libc/*.c 
CFILES += src/memory/common.c 
CFILES += thirdparty/dist/lib/http_parser.o
# TODO: Is USE_MEM_GENERIC out of date? I do not see that file.
# Does that mean we can make USE_MEM_VM an invariant?
ifeq ($(USE_MEM),USE_MEM_GENERIC)
CFILES += src/memory/generic.c
else ifeq ($(USE_MEM),USE_MEM_VM)
CFILES += src/memory/64bit_nix.c
endif

# Flags
CFLAGS += -D${ARCH}
#CFLAGS += -DDEBUG 
#CFLAGS += -DLOG_TO_FILE
CFLAGS += -DNCORES=${NCORES}
CFLAGS += -DPAGE_SIZE=$(PAGE_SIZE)
#CFLAGS += -DPREEMPT_DISABLE
#CFLAGS += -DUSE_HTTP_UVIO #-DUSE_HTTP_SYNC
CFLAGS += -D${USE_MEM}  
#CFLAGS += -DUSE_SYSCALL

LDFLAGS += -Wl,--export-dynamic -ldl -lm
LDFLAGS += -Lthirdparty/dist/lib/

# Configuring JasmineEEE
JSMNCFLAGS += -DJSMN_STATIC 
JSMNCFLAGS += -DJSMN_STRICT

all: clean runtime

runtime:
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${CC_OPTIONS} ${INCLUDES} ${CFLAGS} ${LDFLAGS} ${CFILES} ${JSMNCFLAGS} -L/usr/lib/ -luv $^ -o bin/${BINARY_NAME}

thirdparty:
	@echo "Compiling thirdparty"
	@make --no-print-directory -C thirdparty build

tools:
#	@echo "Compiling tools"
	@make --no-print-directory -C tools

clean:
	@rm -f core
	@echo "Cleaning up runtime"
	@rm -f bin/${BINARY_NAME}
#	@echo "Cleaning up tools"
#	@make --no-print-directory -C tools clean

distclean: clean
	@make --no-print-directory -C thirdparty clean

fetch:
	@git submodule update --init --recursive

init: fetch clean thirdparty runtime

.PHONY: all init clean fetch thirdparty runtime tools
