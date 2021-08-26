WASMCC = ../../../awsm/wasi-sdk/bin/clang
CC = clang
TARGET = wasm32-wasi
SYSROOT = ../../../awsm/wasi-sdk/share/wasi-sysroot
OPTFLAGS=-O3 -flto
SLEDGE_RT_INC=../../include/
SLEDGE_MEMC=../../compiletime/memory/64bit_nix.c
SLEDGE_WASMISA=../../compiletime/instr.c

SLEDGE_BIN_DIR=../../bin/

clean:
	rm -f fibonacci.wasm
	rm -f fibonacci.bc
	rm -f fibonacci.so
	rm -f ../../bin/fibonacci.so

fibonacci.wasm: main.c
	$(WASMCC) -Wl,--allow-undefined,-z,stack-size=32768,--threads=1,--export-all --target=$(TARGET) -mcpu=mvp $(OPTFLAGS) --sysroot=$(SYSROOT) $< -o $@

fibonacci.bc: fibonacci.wasm
	../../../awsm/target/release/awsm  $< -o $@

fibonacci.so: fibonacci.bc
	$(CC) --shared -fPIC $(OPTFLAGS) -I${SLEDGE_RT_INC} ${SLEDGE_MEMC} ${SLEDGE_WASMISA} $< -o $@

.PHONY: run
run: fibonacci.so
	cp $< ../../bin/
	cd ../../bin/ && LD_LIBRARY_PATH=. ./sledgert ../tests/fibonacci/test_fibonacci.json

.PHONY: debug
debug: fibonacci.so
	cp $< ../../bin/
	cd ../../bin/ && \
	LD_LIBRARY_PATH=. gdb \
			--eval-command="handle SIGUSR1 noprint nostop" \
			--eval-command="handle SIGPIPE noprint nostop" \
			--eval-command="set pagination off" \
			--eval-command="run ../tests/fibonacci/test_fibonacci.json" \
			sledgert
