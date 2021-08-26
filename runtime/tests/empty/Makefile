BASE_DIR=../../../

WASMCC = ${BASE_DIR}/awsm/wasi-sdk/bin/clang
AWSM_CC=${BASE_DIR}/awsm/target/release/awsm
CC = clang
TARGET = wasm32-wasi
SYSROOT = ${BASE_DIR}/awsm/wasi-sdk/share/wasi-sysroot
OPTFLAGS=-O3 -flto
SLEDGE_RT_INC=../../include/
SLEDGE_MEMC=../../compiletime/memory/64bit_nix.c
SLEDGE_WASMISA=../../compiletime/instr.c

SLEDGE_BIN_DIR=../../bin/

clean:
	rm -f empty.wasm
	rm -f empty.bc
	rm -f empty.so
	rm -f ../../bin/empty.so

empty.wasm: main.c
	$(WASMCC) -Wl,--allow-undefined,-z,stack-size=32768,--threads=1,--export-all --target=$(TARGET) -mcpu=mvp $(OPTFLAGS) --sysroot=$(SYSROOT) $< -o $@

empty.bc: empty.wasm
	$(AWSM_CC) $< -o $@

empty.so: empty.bc
	$(CC) --shared -fPIC $(OPTFLAGS) -I${SLEDGE_RT_INC} ${SLEDGE_MEMC} ${SLEDGE_WASMISA} $< -o $@

.PHONY: run
run: empty.so
	cp $< ../../bin/
	cd ../../bin/ && LD_LIBRARY_PATH=. ./sledgert ../tests/empty/test_empty.json

.PHONY: debug
debug: empty.so
	cp $< ../../bin/
	cd ../../bin/ && \
	LD_LIBRARY_PATH=. gdb \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run ../tests/empty/test_empty.json" \
		sledgert

