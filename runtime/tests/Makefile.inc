BASE=awsm
SFCC=silverfish
CC=clang
WASMCC=wasm32-unknown-unknown-wasm-clang

OPTFLAGS=-O3 -flto
#same stack-size for all
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

MEMC_64=64bit_nix.c
MEMC_NO=no_protection.c
MEMC_GEN=generic.c
MEMC_MPX=mpx.c
MEMC_SEG=segmented.c

BASE_DIR=../../silverfish/
RT_DIR=${BASE_DIR}/runtime/
RT_MEM=${RT_DIR}/memory/
RT_LIBC=${RT_DIR}/libc/libc_backing.c
RT_RT=${RT_DIR}/runtime.c

MEMC=${RT_MEM}/${MEMC_64}
DUMMY=${BASE_DIR}/code_benches/dummy.c

# for awsm
ABASE_DIR=../../
ART_DIR=${ABASE_DIR}/runtime/
ART_INC=${ART_DIR}/include/

USE_MEM=USE_MEM_VM
ifeq ($(USE_MEM),USE_MEM_VM)
AMEMC=${ART_DIR}/compiletime/memory/${MEMC_64}
endif
WASMISA=${ART_DIR}/compiletime/instr.c

ABIN_DIR=${ART_DIR}/bin/
TMP_DIR=tmp/

