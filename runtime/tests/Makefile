CC=clang # Source -> Native
WASMCC=wasm32-unknown-unknown-wasm-clang # Source -> WebAssembly

OPTFLAGS=-O0 -g
TMP_DIR=tmp/

# same stack-size for all
WASMLINKFLAGS=-Wl,-z,stack-size=524288,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy
WASMCFLAGS=${WASMLINKFLAGS} -nostartfiles

# aWsm Compiler Runtime (WebAssembly -> LLVM Bitcode)
AWSM_NAME=awsm
AWSM_BASE_DIR=../../${AWSM_NAME}
DUMMY=${AWSM_BASE_DIR}/code_benches/dummy.c

# for SLEdge
SLEDGE_BASE_DIR=../../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_COMPILETIME_INC=${SLEDGE_RT_DIR}/include/common
SLEDGE_BIN_DIR=${SLEDGE_RT_DIR}/bin/
SLEDGE_COMPILETIME_SRC=${SLEDGE_RT_DIR}/compiletime/*.c

ALL=fibonacci empty license_plate_detection resize_image gocr cifar10
ALL_COPY_DEST=$(ALL:%=../../runtime/bin/%_wasm.so)

.PHONY: all
all: $(ALL_COPY_DEST)

.PHONY: clean
clean:
	@make clean -C ./CMSIS_5_NN/ -f Makefile
	@make clean -C ./gocr/src/ -f wasm.mk
	@make clean -C ./sod/
	@make clean -C ./TinyEKF/extras/c/ -f wasm.mk
	@rm -f ../../runtime/bin/*.so

cifar10.wasm:
	@make cifar10.wasm -C ./CMSIS_5_NN/ -f Makefile
	@cp ./CMSIS_5_NN/cifar10.wasm cifar10.wasm

gocr.wasm:
	@make gocr.wasm -C ./gocr/src/ -f wasm.mk
	@cp ./gocr/src/gocr.wasm gocr.wasm

resize_image.wasm:
	@make dir resize_image.wasm -C ./sod/
	@cp ./sod/bin/resize_image.wasm resize_image.wasm

license_plate_detection.wasm:
	@make dir license_plate_detection.wasm -C ./sod/
	@cp ./sod/bin/license_plate_detection.wasm license_plate_detection.wasm

gps_ekf_fn.wasm:
	@make gps_ekf_fn.wasm -C ./TinyEKF/extras/c/ -f wasm.mk
	@cp ./TinyEKF/extras/c/gps_ekf_fn.wasm gps_ekf_fn.wasm

%.wasm: %.c $(DUMMY)
	${WASMCC} ${WASMCFLAGS} ${OPTFLAGS} $^ -o $@

%.bc: %.wasm
	${AWSM_NAME} --inline-constant-globals --runtime-globals $^ -o $@

%_wasm.so: %.bc ${SLEDGE_COMPILETIME_SRC}
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_COMPILETIME_INC} $^ -o $@

../../runtime/bin/%.so: %.so
	cp $^ $@
