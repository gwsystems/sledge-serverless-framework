include Makefile.inc

TESTS=fibonacci empty work work1k work10k work100k work1m
TESTS+=forever filesys sockserver sockclient empty

TESTSRT=$(TESTS:%=%_rt)
BENCHES=adpcm basic_math binarytrees bitcount blowfish crc dijkstra fft function_pointers \
        gsm libjpeg mandelbrot patricia pgp qsort rsynth sha sqlite stringsearch susan

.PHONY: all clean rttests

all: rttests
	@echo "Compilation done!"

rttests: $(TESTSRT)

clean:
	@rm -rf ${TMP_DIR}
	#@rm -f ${BIN_DIR}/*_wasm.so

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

