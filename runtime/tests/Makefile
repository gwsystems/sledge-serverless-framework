AWSMCC=awsm
CC=clang

OPTFLAGS=-O3 -flto

# Compiletime Components to link into *.so
SLEDGE_BASE_DIR=../../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_COMPILETIME_INC=${SLEDGE_RT_DIR}/include
SLEDGE_COMPILETIME_SRC=${SLEDGE_RT_DIR}/compiletime/instr.c ${SLEDGE_RT_DIR}/compiletime/memory/64bit_nix.c

ALL=fibonacci empty ekf cifar10 gocr lpd resize
ALL_COPY_DEST=$(ALL:%=../../runtime/bin/%_wasm.so)

.PHONY: all
all: $(ALL_COPY_DEST)

.PHONY: clean
clean:
	@make clean -C ./fibonacci
	@make clean -C ./empty
	@make clean -C ./TinyEKF/extras/c/ -f wasm.mk
	@make clean -C ./CMSIS_5_NN/ -f Makefile
	@make clean -C ./gocr/src/ -f wasm.mk
	@make clean -C ./sod/
	@rm -f ../../runtime/bin/*.so

fibonacci.wasm:
	@make fibonacci.wasm -C ./fibonacci
	@cp ./fibonacci/fibonacci.wasm fibonacci.wasm

empty.wasm:
	@make empty.wasm -C ./empty
	@cp ./empty/empty.wasm empty.wasm

ekf.wasm:
	@make gps_ekf_fn.wasm -C ./TinyEKF/extras/c/ -f wasm.mk
	@cp ./TinyEKF/extras/c/gps_ekf_fn.wasm ekf.wasm

cifar10.wasm:
	@make cifar10.wasm -C ./CMSIS_5_NN/ -f Makefile
	@cp ./CMSIS_5_NN/cifar10.wasm cifar10.wasm

gocr.wasm:
	@make gocr.wasm -C ./gocr/src/ -f wasm.mk
	@cp ./gocr/src/gocr.wasm gocr.wasm

lpd.wasm:
	@make dir license_plate_detection.wasm -C ./sod/
	@cp ./sod/bin/license_plate_detection.wasm lpd.wasm

resize.wasm:
	@make dir resize_image.wasm -C ./sod/
	@cp ./sod/bin/resize_image.wasm resize.wasm

%.bc: %.wasm
	${AWSMCC} --inline-constant-globals --runtime-globals $^ -o $@

%_wasm.so: %.bc ${SLEDGE_COMPILETIME_SRC}
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_COMPILETIME_INC} $^ -o $@

../../runtime/bin/%.so: %.so
	cp $^ $@
