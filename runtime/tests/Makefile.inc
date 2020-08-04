ARCH := $(shell uname -m)

CC=clang # Source -> Native
WASMCC=wasm32-unknown-unknown-wasm-clang # Source -> WebAssembly

OPTFLAGS=-O3 -flto
MEMC_64=64bit_nix.c
# MEMC_NO=no_protection.c
# MEMC_GEN=generic.c
# MEMC_MPX=mpx.c
# MEMC_SEG=segmented.c
TMP_DIR=tmp/

# same stack-size for all
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

# aWsm Compiler Runtime (WebAssembly -> Native)
# Currently, the component which compiles WebAssembly modules to native code, is named aWsm
# However, the executable itself still currently retains the older name silverfish
WASM2OBJ_NAME=awsm
WASM2OBJ_EXECUTABLE=silverfish
WASM2OBJ_BASE_DIR=../../${WASM2OBJ_NAME}
WASM2OBJ_RT_DIR=${WASM2OBJ_BASE_DIR}/runtime/
WASM2OBJ_RT_MEM=${WASM2OBJ_RT_DIR}/memory/
WASM2OBJ_RT_LIBC=${WASM2OBJ_RT_DIR}/libc/libc_backing.c
WASM2OBJ_RT_RT=${WASM2OBJ_RT_DIR}/runtime.c
WASM2OBJ_RT_ENV=${WASM2OBJ_RT_DIR}/libc/env.c
# Is seems like the arch specific code no longer exists
# WASM2OBJ_RT_ENV=${WASM2OBJ_RT_DIR}/libc/env.c ${WASM2OBJ_RT_DIR}/libc/env_${ARCH}.c
WASM2OBJ_MEMC=${WASM2OBJ_RT_MEM}/${MEMC_64}
WASM2OBJ_DUMMY=${WASM2OBJ_BASE_DIR}/code_benches/dummy.c

# for SLEdge
SERVERLESS_BASE_DIR=../../
SERVERLESS_RT_DIR=${SERVERLESS_BASE_DIR}/runtime/
SERVERLESS_RT_INC=${SERVERLESS_RT_DIR}/include/
SERVERLESS_BIN_DIR=${SERVERLESS_RT_DIR}/bin/
SERVERLESS_WASMISA=${SERVERLESS_RT_DIR}/compiletime/instr.c

USE_MEM=USE_MEM_VM

ifeq ($(USE_MEM),USE_MEM_VM)
SERVERLESS_MEMC=${SERVERLESS_RT_DIR}/compiletime/memory/${MEMC_64}
endif
