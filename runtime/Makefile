CC=clang

RTDIR=src/
CFILES=${RTDIR}/*.c ${RTDIR}/libc/*.c ${RTDIR}/memory/common.c
RUNTIME=bin/awsmrt
INC=include/

OPTFLAGS = -O3
OPTFLAGS += -flto -g
LDFLAGS = -Wl,--export-dynamic -ldl -lm
USE_MEM = USE_MEM_VM
CFLAGS = ${OPTFLAGS} -D${USE_MEM} -I${INC} -pthread
CFLAGS += -DX86_64 
#CFLAGS += -DDEBUG 
CFLAGS += -D_GNU_SOURCE
#CFLAGS += -DNOSTDIO
#CFLAGS += -DSTANDALONE
CFLAGS += -DUSE_UVIO
CFLAGS += -DUSE_HTTP_UVIO #-DUSE_HTTP_SYNC
CFLAGS += -DSBOX_SCALE_ALLOC
#CFLAGS += -DUSE_SYSCALL
#CFLAGS += -DPREEMPT_DISABLE
CACHE_LINESIZE := $(shell getconf LEVEL1_DCACHE_LINESIZE)
NCORES_CONF := $(shell getconf _NPROCESSORS_CONF)
#todo:cross-compile
CFLAGS += -DCACHELINE_SIZE=${CACHE_LINESIZE}
CFLAGS += -DNCORES=${NCORES_CONF}
#CFLAGS += -DNCORES=4
CFLAGS += -DPAGE_SIZE=$(shell getconf PAGESIZE)

MAKE= make --no-print-directory

JSMN=jsmn
JSMNINC=-I${JSMN}
JSMNCFLAGS=${JSMNINC} -DJSMN_STATIC -DJSMN_STRICT

HTTPPARSE=http-parser
HTTPPARSEINC=-I${HTTPPARSE}
HTTPPARSESRC=${HTTPPARSE}/http_parser.c
HTTPPARSECFLAGS=${HTTPPARSEINC}
CFILES += ${HTTPPARSESRC}

ifeq ($(USE_MEM),USE_MEM_GENERIC)
CFILES += ${RTDIR}/memory/generic.c
else ifeq ($(USE_MEM),USE_MEM_VM)
CFILES += ${RTDIR}/memory/64bit_nix.c
endif

all: clean runtime #tools

runtime:
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${CFLAGS} ${LDFLAGS} ${CFILES} ${JSMNCFLAGS} ${HTTPPARSECFLAGS} -L/usr/lib/ -luv $^ -o ${RUNTIME}

tools:
#	@echo "Compiling tools"
	@${MAKE} -C tools

clean:
	@rm -f core
	@echo "Cleaning up runtime"
	@rm -f ${RUNTIME}
#	@echo "Cleaning up tools"
	@${MAKE} -C tools clean

fetch:
	@git submodule update --init --recursive

init: fetch clean runtime tools

.PHONY: all init clean fetch libuv runtime tools
