WASMCC=${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot/
CC=clang
WASMLINKFLAGS=-Wl,--allow-undefined,-z,stack-size=32768,--threads=1
OPTFLAGS=-O3 -flto

exit.out: main.c
	@${CC} ${OPTFLAGS} $^ -o $@

exit.wasm: main.c
	@${WASMCC} ${WASMLINKFLAGS} ${OPTFLAGS} $^ -o $@

.PHONY: clean
clean:
	@rm -f exit.wasm 
	@rm -f exit.out

# Should return 0 exit status
.PHONY: run
run: exit.wasm
	@wasmtime exit.wasm

.PHONY: run_native
run_native: exit.out
	@./exit.out

