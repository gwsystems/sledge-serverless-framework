CC=clang

RTDIR=src/
CFILES=${RTDIR}/*.c ${RTDIR}/libc/*.c ${RTDIR}/memory/common.c
RUNTIME=bin/awsmrt
INC=include/
ARCH := $(shell uname -m)

OPTFLAGS = -O3
OPTFLAGS += -flto -g
LDFLAGS = -Wl,--export-dynamic -ldl -lm
USE_MEM = USE_MEM_VM
CFLAGS = ${OPTFLAGS} -D${USE_MEM} -I${INC} -pthread
CFLAGS += -D${ARCH}
CFILES += ${RTDIR}/arch/${ARCH}/*.c
CFLAGS += -DDEBUG 
CFLAGS += -D_GNU_SOURCE
#CFLAGS += -DLOG_TO_FILE
#CFLAGS += -DUSE_HTTP_UVIO #-DUSE_HTTP_SYNC
#CFLAGS += -DUSE_SYSCALL
#CFLAGS += -DPREEMPT_DISABLE
NCORES_CONF := $(shell getconf _NPROCESSORS_CONF)
#todo:cross-compile
CFLAGS += -DNCORES=${NCORES_CONF}
#CFLAGS += -DNCORES=4
CFLAGS += -DPAGE_SIZE=$(shell getconf PAGESIZE)

MAKE= make --no-print-directory
THIRDPARTY=thirdparty

THIRDPARTY_DIST=${THIRDPARTY}/dist/
THIRDPARTY_INC=${THIRDPARTY_DIST}/include/
THIRDPARTY_LIB=${THIRDPARTY_DIST}/lib/
LDFLAGS += -L${THIRDPARTY_LIB}
CFLAGS += -I${THIRDPARTY_INC}

JSMNCFLAGS=-DJSMN_STATIC -DJSMN_STRICT
HTTPPARSEOBJ=${THIRDPARTY_LIB}/http_parser.o
CFILES += ${HTTPPARSEOBJ}

ifeq ($(USE_MEM),USE_MEM_GENERIC)
CFILES += ${RTDIR}/memory/generic.c
else ifeq ($(USE_MEM),USE_MEM_VM)
CFILES += ${RTDIR}/memory/64bit_nix.c
endif

all: clean runtime

runtime:
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${CFLAGS} ${LDFLAGS} ${CFILES} ${JSMNCFLAGS} ${HTTPPARSECFLAGS} -L/usr/lib/ -luv $^ -o ${RUNTIME}

thirdparty:
	@echo "Compiling thirdparty"
	@${MAKE} -C thirdparty build

tools:
#	@echo "Compiling tools"
	@${MAKE} -C tools

clean:
	@rm -f core
	@echo "Cleaning up runtime"
	@rm -f ${RUNTIME}
#	@echo "Cleaning up tools"
#	@${MAKE} -C tools clean

distclean: clean
	@${MAKE} -C thirdparty clean

fetch:
	@git submodule update --init --recursive

init: fetch clean thirdparty runtime

.PHONY: all init clean fetch thirdparty runtime tools
