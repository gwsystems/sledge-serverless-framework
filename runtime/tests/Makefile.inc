BASE=awsm
SFCC=silverfish
CC=clang
WASMCC=wasm32-unknown-unknown-wasm-clang

OPTFLAGS=-O3 -flto
#same stack-size for all
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

BASE_DIR=../../
RT_DIR=${BASE_DIR}/runtime/
RT_INC=${RT_DIR}/include/

USE_MEM=USE_MEM_VM

ifeq ($(USE_MEM),USE_MEM_VM)
MEMC=${RT_DIR}/compiletime/memory/64bit_nix.c
endif

BIN_DIR=${RT_DIR}/bin/
TMP_DIR=tmp/
