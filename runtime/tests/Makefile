AWSMCC=awsm
CC=clang

OPTFLAGS=-O3 -flto

# Compiletime Components to link into *.so
SLEDGE_BASE_DIR=../../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_COMPILETIME_INC=${SLEDGE_RT_DIR}/include
SLEDGE_COMPILETIME_SRC=${SLEDGE_RT_DIR}/compiletime/*.c

.PHONY: all
all: fibonacci.install empty.install ekf.install cifar10.install gocr.install lpd.install resize.install

.PHONY: clean
clean:
	@make clean -C ./fibonacci
	@make clean -C ./empty
	@make clean -C ./TinyEKF/ -f wasm.mk
	@make clean -C ./CMSIS_5_NN/ -f Makefile
	@make clean -C ./gocr/ -f wasm.mk
	@make clean -C ./sod/
	@rm -f *.wasm
	@rm -f ../../runtime/bin/*.so

%.bc: %.wasm
	${AWSMCC} --inline-constant-globals --runtime-globals $^ -o $@

%_wasm.so: %.bc ${SLEDGE_COMPILETIME_SRC}
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_COMPILETIME_INC} $^ -o $@

../../runtime/bin/%.so: %.so
	cp $^ $@

# Fibonacci
./fibonacci/fibonacci.wasm:
	@make fibonacci.wasm -C ./fibonacci

fibonacci.wasm: ./fibonacci/fibonacci.wasm
	@cp ./fibonacci/fibonacci.wasm fibonacci.wasm

.PHONY: fibonacci.install
fibonacci.install: ../../runtime/bin/fibonacci_wasm.so

# Empty
./empty/empty.wasm:
	@make empty.wasm -C ./empty

empty.wasm: ./empty/empty.wasm
	@cp ./empty/empty.wasm empty.wasm

.PHONY: empty.install
empty.install: ../../runtime/bin/empty_wasm.so

# Exit
./exit/exit.wasm:
	@make exit.wasm -C ./exit

exit.wasm: ./exit/exit.wasm
	@cp ./exit/exit.wasm exit.wasm

.PHONY: exit.install
exit.install: ../../runtime/bin/exit_wasm.so

# EKF
./TinyEKF/gps_ekf.wasm:
	@make gps_ekf.wasm -C ./TinyEKF/ -f wasm.mk

ekf.wasm: ./TinyEKF/gps_ekf.wasm
	@cp ./TinyEKF/gps_ekf.wasm ekf.wasm

.PHONY: ekf.install
ekf.install: ../../runtime/bin/ekf_wasm.so

# CIFAR10
./CMSIS_5_NN/cifar10.wasm:
	@make cifar10.wasm -C ./CMSIS_5_NN/ -f Makefile

cifar10.wasm: ./CMSIS_5_NN/cifar10.wasm
	@cp ./CMSIS_5_NN/cifar10.wasm cifar10.wasm

.PHONY: cifar10.install
cifar10.install: ../../runtime/bin/cifar10_wasm.so

# GOCR
./gocr/gocr.wasm:
	@make gocr.wasm -C ./gocr/ -f wasm.mk

gocr.wasm: ./gocr/gocr.wasm
	@cp ./gocr/gocr.wasm gocr.wasm

.PHONY: gocr.install
gocr.install: ../../runtime/bin/gocr_wasm.so

# LPD
./sod/license_plate_detection.wasm:
	@make license_plate_detection.wasm -C ./sod/

lpd.wasm: ./sod/license_plate_detection.wasm
	@cp ./sod/license_plate_detection.wasm lpd.wasm

.PHONY: lpd.install
lpd.install: ../../runtime/bin/lpd_wasm.so

# Resize
./sod/resize_image.wasm:
	@make resize_image.wasm -C ./sod/

resize.wasm: ./sod/resize_image.wasm
	@cp ./sod/resize_image.wasm resize.wasm

.PHONY: resize.install
resize.install: ../../runtime/bin/resize_wasm.so
