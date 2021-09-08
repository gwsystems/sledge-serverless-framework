include Makefile.inc

TESTS=fibonacci empty work work1k work10k work100k work1m forever filesys sockserver sockclient empty

TESTSRT=$(TESTS:%=%_rt)

.PHONY: all clean rttests tinyekf cifar10 gocr sod

all: rttests tinyekf cifar10 gocr sod
	@echo "Test Compilation done!"

rttests: $(TESTSRT)

clean:
	@echo "Cleaning Test Applications"
	@make clean -C ./empty/
	@rm -f wasm/empty.wasm bc/empty.bc so/empty.so
	@rm -rf ${SLEDGE_BIN_DIR}/empty.so ${SLEDGE_BIN_DIR}/test_empty.json

	@make clean -C ./echo/
	@rm -f wasm/echo.wasm bc/echo.bc so/echo.so
	@rm -rf ${SLEDGE_BIN_DIR}/echo.so ${SLEDGE_BIN_DIR}/test_echo.json

	@make clean -C ./fibonacci/
	@rm -f wasm/fibonacci.wasm bc/fibonacci.bc so/fibonacci.so
	@rm -rf ${SLEDGE_BIN_DIR}/fibonacci.so ${SLEDGE_BIN_DIR}/test_fibonacci.json
	
	@make clean -C ./TinyEKF/ -f wasm.mk
	@rm -f wasm/gps_ekf.wasm bc/gps_ekf.bc so/gps_ekf.so
	@rm -rf ${SLEDGE_BIN_DIR}/gps_ekf.so ${SLEDGE_BIN_DIR}/test_gps_ekf.json

tinyekf:
	@echo "Making and Installing tinyekf"
	@make gps_ekf_fn.so -C ./TinyEKF/extras/c/ -f wasm.mk
	@cp ./TinyEKF/extras/c/gps_ekf_fn.so ${SLEDGE_BIN_DIR}/ekf_wasm.so

cifar10:
	@echo "Making and Installing cifar10"
	@make cifar10.so -C ./CMSIS_5_NN/ -f Makefile
	@cp ./CMSIS_5_NN/cifar10.so ${SLEDGE_BIN_DIR}/cifar10_wasm.so

gocr:
	@echo "Making and Installing gocr"
	@make gocr.so -C ./gocr/src/ -f wasm.mk
	@cp ./gocr/src/gocr.so ${SLEDGE_BIN_DIR}/gocr_wasm.so

sod:
	@echo "Making and Installing license_plate_detection and image_resize"
	@make dir samples.so -C ./sod/
	@cp ./sod/bin/license_plate_detection.so ${SLEDGE_BIN_DIR}/lpd_wasm.so
	@cp ./sod/bin/resize_image.so ${SLEDGE_BIN_DIR}/resize_wasm.so

%_rt:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(AWSM_DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm
	${AWSM_NAME} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SLEDGE_MEMC} ${SLEDGE_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${SLEDGE_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

