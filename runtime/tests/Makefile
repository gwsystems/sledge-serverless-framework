BASE_DIR=../../
AWSM_CC=${BASE_DIR}/awsm/target/release/awsm
CC = clang
OPTFLAGS=-O3 -flto

SLEDGE_RT_DIR=${BASE_DIR}/runtime/
SLEDGE_RT_INC=${SLEDGE_RT_DIR}/include/
SLEDGE_BIN_DIR=${SLEDGE_RT_DIR}/bin/
SLEDGE_WASMISA=${SLEDGE_RT_DIR}/compiletime/instr.c
SLEDGE_MEMC=${SLEDGE_RT_DIR}/compiletime/memory/64bit_nix.c

wasm/empty.wasm:
	mkdir -p wasm
	make empty.wasm -C empty
	cp empty/empty.wasm wasm/empty.wasm

wasm/fibonacci.wasm:
	mkdir -p wasm
	make fibonacci.wasm -C fibonacci
	cp fibonacci/fibonacci.wasm wasm/fibonacci.wasm

wasm/gps_ekf.wasm:
	mkdir -p wasm
	make gps_ekf.wasm -C TinyEKF -f wasm.mk
	cp TinyEKF/gps_ekf.wasm wasm/gps_ekf.wasm

wasm/cifar10.wasm:
	mkdir -p wasm
	make cifar10.wasm -C CMSIS_5_NN
	cp ./CMSIS_5_NN/cifar10.wasm wasm/cifar10.wasm

wasm/gocr.wasm:
	mkdir -p wasm
	make gocr.wasm -C gocr -f wasm.mk
	cp ./gocr/gocr.wasm wasm/gocr.wasm

wasm/resize_image.wasm:
	mkdir -p wasm
	make resize_image.wasm -C ./sod/
	cp ./sod/resize_image.wasm wasm/resize_image.wasm

wasm/license_plate_detection.wasm:
	mkdir -p wasm
	make license_plate_detection.wasm -C ./sod/
	cp ./sod/license_plate_detection.wasm wasm/license_plate_detection.wasm

bc/%.bc: wasm/%.wasm
	mkdir -p bc
	${AWSM_CC} $< -o $@

so/%.so: bc/%.bc
	mkdir -p so
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} ${SLEDGE_MEMC} ${SLEDGE_WASMISA} $< -o $@

.PHONY: %.install
%.install: so/%.so json/test_%.json
	cp $^ -t ../bin/

.PHONY: %.run
%.run: %.install
	cd ../bin/ && LD_LIBRARY_PATH=. ./sledgert ./test_${@:.run=.json}

.PHONY: %.debug
%.debug: %.install
	cd ../bin/ && LD_LIBRARY_PATH=. gdb \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run ./test_${@:.debug=.json}" \
		sledgert

.PHONY: all 
all: empty.install fibonacci.install gps_ekf.install cifar10.install gocr.install resize_image.install license_plate_detection.install 
	@echo "Test Compilation done!"

.PHONY: clean
clean:
	@echo "Cleaning Test Applications"
	@make clean -C ./empty/
	@rm -f wasm/empty.wasm bc/empty.bc so/empty.so
	@rm -rf ${SLEDGE_BIN_DIR}/empty.so ${SLEDGE_BIN_DIR}/test_empty.json

	@make clean -C ./fibonacci/
	@rm -f wasm/fibonacci.wasm bc/fibonacci.bc so/fibonacci.so
	@rm -rf ${SLEDGE_BIN_DIR}/fibonacci.so ${SLEDGE_BIN_DIR}/test_fibonacci.json
	
	@make clean -C ./TinyEKF/ -f wasm.mk
	@rm -f wasm/gps_ekf.wasm bc/gps_ekf.bc so/gps_ekf.so
	@rm -rf ${SLEDGE_BIN_DIR}/gps_ekf.so ${SLEDGE_BIN_DIR}/test_gps_ekf.json

	@make clean -C ./CMSIS_5_NN/
	@rm -f wasm/cifar10.wasm bc/cifar10.bc so/cifar10.so
	@rm -rf ${SLEDGE_BIN_DIR}/cifar10.so ${SLEDGE_BIN_DIR}/test_cifar10.json

	@make clean -C ./gocr/ -f wasm.mk
	@rm -f wasm/gocr.wasm bc/gocr.bc so/gocr.so
	@rm -rf ${SLEDGE_BIN_DIR}/gocr.so ${SLEDGE_BIN_DIR}/test_gocr.json
	
	@make clean -C ./sod/
	@rm -f wasm/resize_image.wasm bc/resize_image.bc so/resize_image.so
	@rm -rf ${SLEDGE_BIN_DIR}/resize_image.so ${SLEDGE_BIN_DIR}/test_resize_image.json
	@rm -f wasm/license_plate_detection.wasm bc/license_plate_detection.bc so/license_plate_detection.so
	@rm -rf ${SLEDGE_BIN_DIR}/license_plate_detection.so ${SLEDGE_BIN_DIR}/test_license_plate_detection.json
