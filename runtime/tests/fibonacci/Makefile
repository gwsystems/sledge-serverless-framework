WASMCC = ../../../awsm/wasi-sdk/bin/clang
CC = clang
TARGET = wasm32-wasi
SYSROOT = ../../../awsm/wasi-sdk/share/wasi-sysroot
OPTFLAGS=-O3 -flto
SLEDGE_RT_INC=../../include/
SLEDGE_MEMC=../../compiletime/memory/64bit_nix.c
SLEDGE_WASMISA=../../compiletime/instr.c
USE_MEM=USE_MEM_VM

SLEDGE_BIN_DIR=../../bin/

clean:
	rm -f fibonacci.wasm
	rm -f fibonacci.bc
	rm -f fibonacci.so

fibonacci.wasm: main.c
	$(WASMCC) -Wl,--allow-undefined,-z,stack-size=32768,--threads=1 --target=$(TARGET) -mcpu=mvp $(OPTFLAGS) --sysroot=$(SYSROOT) $< -o $@

fibonacci.bc: fibonacci.wasm
	../../../awsm/target/release/awsm  $< -o $@

fibonacci.so: fibonacci.bc
	$(CC) --shared -fPIC $(OPTFLAGS) -D${USE_MEM} -I${SLEDGE_RT_INC} ${SLEDGE_MEMC} ${SLEDGE_WASMISA} $< -o $@

.PHONE: run
run: fibonacci.so
	cp $< ../../bin/
	cp test_fibonacci.json ../../bin/
	cd ../../bin/ && LD_LIBRARY_PATH=. ./sledgert test_fibonacci.json

# ${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SLEDGE_MEMC} ${SLEDGE_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
