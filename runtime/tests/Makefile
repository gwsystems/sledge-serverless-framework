include Makefile.inc

TESTS=fibonacci empty work work1k work10k work100k work1m forever filesys sockserver sockclient empty

TESTSRT=$(TESTS:%=%_rt)

.PHONY: all clean rttests tinyekf cifar10 gocr sod

all: rttests tinyekf cifar10 gocr sod
	@echo "Compilation done!"

rttests: $(TESTSRT)

clean:
	@rm -rf ${TMP_DIR}
	@rm -rf ${SLEDGE_BIN_DIR}/*_wasm.so

tinyekf:
	make gps_ekf_fn.so -C ./TinyEKF/extras/c/ -f wasm.mk
	cp ./TinyEKF/extras/c/gps_ekf_fn.so ${SLEDGE_BIN_DIR}/ekf_wasm.so

cifar10:
	make cifar10.so -C ./CMSIS_5_NN/ -f Makefile
	cp ./CMSIS_5_NN/cifar10.so ${SLEDGE_BIN_DIR}/cifar10_wasm.so

gocr:
	make gocr.so -C ./gocr/src/ -f wasm.mk
	cp ./gocr/src/gocr.so ${SLEDGE_BIN_DIR}/gocr_wasm.so

sod:
	make clean dir samples.so -C ./sod/
	cp ./sod/bin/license_plate_detection.so ${SLEDGE_BIN_DIR}/lpd_wasm.so
	cp ./sod/bin/resize_image.so ${SLEDGE_BIN_DIR}/resize_wasm.so

%_rt:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(AWSM_DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm
	${AWSM_NAME} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SLEDGE_MEMC} ${SLEDGE_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${SLEDGE_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

