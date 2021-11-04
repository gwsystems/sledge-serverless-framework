WASMCC=${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot/
WASMLINKFLAGS=-Wl,--allow-undefined,-z,stack-size=32768,--threads=1
CC=clang
OPTFLAGS=-O3 -flto

echo.wasm: main.c
	@${WASMCC} ${WASMLINKFLAGS} ${OPTFLAGS} $^ -o $@

echo.out: main.c
	@${CC} ${OPTFLAGS} $^ -o $@

.PHONY: clean
clean:
	@rm -f echo.wasm

# Serverless style app that uses stdin and stdout
# echo "10" | make run
.PHONY: run
run: echo.wasm
	@wasmtime echo.wasm

.PHONY: run_native
run_native: echo.out
	@./echo.out
