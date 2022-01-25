AWSMCC=../awsm/target/release/awsm
CC=clang-8

# Used by aWsm when compiling the *.wasm to *.bc
AWSMFLAGS= --inline-constant-globals --runtime-globals

# Used by clang when compiling the *.so module
CFLAGS=-O3
LDFLAGS=--shared -fPIC -flto

# Compiletime Components to link into *.so
SLEDGE_BASE_DIR=../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_COMPILETIME_INC=${SLEDGE_RT_DIR}/include
SLEDGE_COMPILETIME_SRC=${SLEDGE_RT_DIR}/compiletime/*.c

.PHONY: all
all: \
	cifar10.install \
	empty.install \
	ekf.install \
	fibonacci.install \
	gocr.install \
	lpd.install \
	resize.install

.PHONY: clean
clean:
	@make clean -C ./wasmception_apps/fibonacci
	@make clean -C ./wasmception_apps/empty
	@make clean -C ./wasmception_apps/TinyEKF/extras/c/ -f wasm.mk
	@make clean -C ./wasmception_apps/CMSIS_5_NN/ -f Makefile
	@make clean -C ./wasmception_apps/gocr/src/ -f wasm.mk
	@make clean -C ./wasmception_apps/sod/
	@rm -f *.wasm
	@rm -rf dist
	@rm -f ../runtime/bin/*.so

dist:
	mkdir -p dist

%.bc: %.wasm dist
	${AWSMCC} ${AWSMFLAGS} $< -o $@

%_wasm.so: %.bc ${SLEDGE_COMPILETIME_SRC}
	mkdir -p dist
	${CC} ${CFLAGS} ${LDFLAGS} -I${SLEDGE_COMPILETIME_INC} $^ -o $@

../runtime/bin/%_wasm.so: dist/%_wasm.so
	cp $^ $@

# Fibonacci
./wasmception_apps/fibonacci/fibonacci.wasm:
	@make fibonacci.wasm -C ./wasmception_apps/fibonacci

dist/fibonacci.wasm: ./wasmception_apps/fibonacci/fibonacci.wasm dist
	@cp ./wasmception_apps/fibonacci/fibonacci.wasm dist/fibonacci.wasm

.PHONY: fibonacci.install
fibonacci.install: ../runtime/bin/fibonacci_wasm.so

# Empty
./wasmception_apps/empty/empty.wasm:
	@make empty.wasm -C ./wasmception_apps/empty

dist/empty.wasm: ./wasmception_apps/empty/empty.wasm dist
	@cp ./wasmception_apps/empty/empty.wasm dist/empty.wasm

.PHONY: empty.install
empty.install: ../runtime/bin/empty_wasm.so

# EKF
./wasmception_apps/TinyEKF/extras/c/gps_ekf_fn.wasm:
	@make gps_ekf_fn.wasm -C ./wasmception_apps/TinyEKF/extras/c/ -f wasm.mk

dist/ekf.wasm: ./wasmception_apps/TinyEKF/extras/c/gps_ekf_fn.wasm dist
	@cp ./wasmception_apps/TinyEKF/extras/c/gps_ekf_fn.wasm dist/ekf.wasm

.PHONY: ekf.install
ekf.install: ../runtime/bin/ekf_wasm.so

# CIFAR10
./wasmception_apps/CMSIS_5_NN/cifar10.wasm:
	@make cifar10.wasm -C ./wasmception_apps/CMSIS_5_NN/ -f Makefile

dist/cifar10.wasm: ./wasmception_apps/CMSIS_5_NN/cifar10.wasm dist
	@cp ./wasmception_apps/CMSIS_5_NN/cifar10.wasm dist/cifar10.wasm

.PHONY: cifar10.install
cifar10.install: ../runtime/bin/cifar10_wasm.so

# GOCR
./wasmception_apps/gocr/src/gocr.wasm:
	@make gocr.wasm -C ./wasmception_apps/gocr/src/ -f wasm.mk

dist/gocr.wasm: ./wasmception_apps/gocr/src/gocr.wasm dist
	@cp ./wasmception_apps/gocr/src/gocr.wasm dist/gocr.wasm

.PHONY: gocr.install
gocr.install: ../runtime/bin/gocr_wasm.so

# LPD
./wasmception_apps/sod/bin/license_plate_detection.wasm:
	@make dir license_plate_detection.wasm -C ./wasmception_apps/sod/

dist/lpd.wasm: ./wasmception_apps/sod/bin/license_plate_detection.wasm dist
	@cp ./wasmception_apps/sod/bin/license_plate_detection.wasm dist/lpd.wasm

.PHONY: lpd.install
lpd.install: ../runtime/bin/lpd_wasm.so

# Resize
./wasmception_apps/sod/bin/resize_image.wasm:
	@make dir resize_image.wasm -C ./wasmception_apps/sod/

dist/resize.wasm: ./wasmception_apps/sod/bin/resize_image.wasm dist
	@cp ./wasmception_apps/sod/bin/resize_image.wasm dist/resize.wasm

.PHONY: resize.install
resize.install: ../runtime/bin/resize_wasm.so
