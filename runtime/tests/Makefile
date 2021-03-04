include Makefile.inc

TESTS=fibonacci empty work work1k work10k work100k work1m forever filesys sockserver sockclient empty

TESTSRT=$(TESTS:%=%_rt)

.PHONY: all clean rttests

all: rttests tinyekf cifar10 gocr sod
	@echo "Compilation done!"

rttests: $(TESTSRT)

clean:
	@rm -rf ${TMP_DIR}
	@rm -rf ${SLEDGE_BIN_DIR}/*_wasm.so

tinyekf:
	make clean all -C ./TinyEKF/extras/c/ -f makefile.wasm
	cp ./TinyEKF/extras/c/gps_ekf_fn.aso ${SLEDGE_BIN_DIR}/ekf_wasm.so

cifar10:
	make clean all -C ./CMSIS_5_NN/ -f Makefile
	cp ./CMSIS_5_NN/tmp/cifar10.awsm.so ${SLEDGE_BIN_DIR}/cifar10_wasm.so

gocr:
	make clean all -C ./gocr/src/ -f makefile.wasm
	cp ./gocr/src/gocr.aso ${SLEDGE_BIN_DIR}/gocr_wasm.so

sod:
	make clean all samples samples.wasm -C ./sod/
	cp ./sod/bin/license_plate_detection.awsm /sledge/runtime/bin/lpd_wasm.so
	cp ./sod/bin/resize_image.awsm /sledge/runtime/bin/resize_wasm.so

%_rt:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(AWSM_DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm
	${AWSM_NAME} ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} ${CFLAGS} ${OPTFLAGS} -D${USE_MEM} -D${ARCH} ${TMP_DIR}/$(@:%_rt=%).bc ${AWSM_MEMC} ${AWSM_RT_LIBC} ${AWSM_RT_ENV} ${AWSM_RT_RT} -lm -o ${TMP_DIR}/$(@:%_rt=%)_wasm.out
	${AWSM_NAME} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} --shared -fPIC ${OPTFLAGS} -I${SLEDGE_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SLEDGE_MEMC} ${SLEDGE_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${SLEDGE_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

