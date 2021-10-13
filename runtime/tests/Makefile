include Makefile.inc

TESTS=fibonacci empty empty

TESTSRT=$(TESTS:%=%_rt)

.PHONY: all
all: rttests tinyekf cifar10 gocr sod
	@echo "Test Compilation done!"

.PHONY: rttests
rttests: $(TESTSRT)

.PHONY: clean
clean:
	@echo "Cleaning Test Applications"
	@rm -rf ${TMP_DIR}
	@make clean -C ./gocr/src/ -f wasm.mk
	@make clean -C ./sod/

.PHONY: tinyekf
tinyekf:
	@echo "Making and Installing tinyekf"
	@make gps_ekf_fn.so -C ./TinyEKF/extras/c/ -f wasm.mk
	@cp ./TinyEKF/extras/c/gps_ekf_fn.so ${SLEDGE_BIN_DIR}/ekf_wasm.so

.PHONY: cifar10
cifar10:
	@echo "Making and Installing cifar10"
	@make cifar10.so -C ./CMSIS_5_NN/ -f Makefile
	@cp ./CMSIS_5_NN/cifar10.so ${SLEDGE_BIN_DIR}/cifar10_wasm.so

.PHONY: gocr
gocr:
	@echo "Making and Installing gocr"
	@make gocr.so -C ./gocr/src/ -f wasm.mk
	@cp ./gocr/src/gocr.so ${SLEDGE_BIN_DIR}/gocr_wasm.so

.PHONY: sod
sod:
	@echo "Making and Installing license_plate_detection and image_resize"
	@make dir samples.so -C ./sod/
	@cp ./sod/bin/license_plate_detection.so ${SLEDGE_BIN_DIR}/lpd_wasm.so
	@cp ./sod/bin/resize_image.so ${SLEDGE_BIN_DIR}/resize_wasm.so

%_rt:
	@mkdir -p ${TMP_DIR}
	# Compile the wasm file
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(AWSM_DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm

	# Compile the *.bc file

	${AWSM_NAME} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	# Compile the *.so file
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SLEDGE_MEMC} ${SLEDGE_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	# Copy the *.so file to the binary directory
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${SLEDGE_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

