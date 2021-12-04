WASMCC=wasm32-unknown-unknown-wasm-clang
OPTFLAGS=-O3 -flto
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

all: fibonacci.wasm

clean:
	rm -rf fibonacci.wasm

fibonacci.wasm: *.c
	${WASMCC} ${WASMCFLAGS} ${OPTFLAGS} $^ -o $@
