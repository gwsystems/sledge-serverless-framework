WASMCC=/opt/wasi-sdk/bin/clang
WASMLINKFLAGS=-Wl,--allow-undefined,-z,stack-size=32768,--threads=1
CC=clang
OPTFLAGS=-O3 -flto

fibonacci.wasm: main.c
	@${WASMCC} ${WASMLINKFLAGS} ${OPTFLAGS} $^ -o $@

fibonacci.wat: fibonacci.wasm
	wasm2wat fibonacci.wasm -o fibonacci.wat

fibonacci.out: main.c
	@${CC} ${OPTFLAGS} $^ -o $@

.PHONY: clean
clean:
	@rm -f fibonacci.wasm

# Serverless style app that uses stdin and stdout
# echo "10" | make run
.PHONY: run
run: fibonacci.wasm
	@wasmtime fibonacci.wasm

.PHONY: run_native
run_native: fibonacci.out
	@./fibonacci.out
