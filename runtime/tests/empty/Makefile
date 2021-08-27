WASMCC=/opt/wasi-sdk/bin/clang
CC=clang
WASMLINKFLAGS=-Wl,--allow-undefined,-z,stack-size=32768,--threads=1
OPTFLAGS=-O3 -flto

empty.out: main.c
	@${CC} ${OPTFLAGS} $^ -o $@

empty.wasm: main.c
	@${WASMCC} ${WASMLINKFLAGS} ${OPTFLAGS} $^ -o $@

.PHONY: clean
clean:
	@rm -f empty.wasm 
	@rm -f empty.out

# Should return 0 exit status
.PHONY: run
run: empty.wasm
	@wasmtime empty.wasm

.PHONY: run_native
run_native: empty.out
	@./empty.out

