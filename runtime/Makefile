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
CFLAGS += -DSTANDALONE
#CFLAGS += -DUSE_UVIO
CFLAGS += -DUSE_SYSCALL
CFLAGS += -DPREEMPT_DISABLE

MAKE= make --no-print-directory

JSMN=jsmn
JSMNINC=-I${JSMN}
JSMNCFLAGS=${JSMNINC} -DJSMN_STATIC -DJSMN_STRICT

ifeq ($(USE_MEM),USE_MEM_GENERIC)
CFILES += ${RTDIR}/memory/generic.c
else ifeq ($(USE_MEM),USE_MEM_VM)
CFILES += ${RTDIR}/memory/64bit_nix.c
endif

all: clean runtime tools

runtime:
	@echo "Compiling runtime"
	@mkdir -p bin/
	@${CC} ${CFLAGS} ${LDFLAGS} ${CFILES} ${JSMNCFLAGS} -L/usr/lib/ -luv $^ -o ${RUNTIME}

tools:
#	@echo "Compiling tools"
	@${MAKE} -C tools

clean:
	@echo "Cleaning up runtime"
	@rm -f ${RUNTIME}
#	@echo "Cleaning up tools"
	@${MAKE} -C tools clean

fetch:
	@git submodule update --init --recursive

init: fetch clean runtime tools

.PHONY: all init clean fetch libuv runtime tools
