ARCH := $(shell uname -m)

CC=clang # Source -> Native
WASMCC=wasm32-unknown-unknown-wasm-clang # Source -> WebAssembly

# OPTFLAGS=-O3 -flto
OPTFLAGS=-O0 -flto -g
MEMC_64=64bit_nix.c
# MEMC_NO=no_protection.c
# MEMC_GEN=generic.c
# MEMC_MPX=mpx.c
# MEMC_SEG=segmented.c
TMP_DIR=tmp/

# same stack-size for all
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

# aWsm Compiler Runtime (WebAssembly -> LLVM Bitcode)
AWSM_NAME=awsm
AWSM_BASE_DIR=../../${AWSM_NAME}
AWSM_RT_DIR=${AWSM_BASE_DIR}/runtime/
AWSM_RT_MEM=${AWSM_RT_DIR}/memory/
AWSM_RT_LIBC=${AWSM_RT_DIR}/libc/libc_backing.c
AWSM_RT_RT=${AWSM_RT_DIR}/runtime.c
AWSM_RT_ENV=${AWSM_RT_DIR}/libc/env.c
# Is seems like the arch specific code no longer exists
# AWSM_RT_ENV=${AWSM_RT_DIR}/libc/env.c ${AWSM_RT_DIR}/libc/env_${ARCH}.c
AWSM_MEMC=${AWSM_RT_MEM}/${MEMC_64}
AWSM_DUMMY=${AWSM_BASE_DIR}/code_benches/dummy.c

# for SLEdge
SLEDGE_BASE_DIR=../../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_RT_INC=${SLEDGE_RT_DIR}/include/
SLEDGE_BIN_DIR=${SLEDGE_RT_DIR}/bin/
SLEDGE_WASMISA=${SLEDGE_RT_DIR}/compiletime/instr.c

USE_MEM=USE_MEM_VM

ifeq ($(USE_MEM),USE_MEM_VM)
SLEDGE_MEMC=${SLEDGE_RT_DIR}/compiletime/memory/${MEMC_64}
endif
