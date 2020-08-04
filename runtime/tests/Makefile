include Makefile.inc

BENCH_DIR=../../${WASM2OBJ_NAME}/code_benches

TESTS=fibonacci empty work work1k work10k work100k work1m
#TESTS=forever filesys sockserver sockclient empty
TESTSRT=$(TESTS:%=%_rt)
BENCHES=basic_math binarytrees bitcount blowfish crc dijkstra fft function_pointers \
        gsm libjpeg mandelbrot patricia pgp qsort rsynth sha sqlite stringsearch susan
#BENCHES=adpcm basic_math binarytrees bitcount blowfish crc dijkstra fft function_pointers \
        gsm libjpeg mandelbrot patricia pgp qsort rsynth sha sqlite stringsearch susan
BENCHESSF=$(BENCHES:%=%_sf)

adpcm_CFLAGS=-Wno-implicit-int -Wno-implicit-function-declaration
crc_CFLAGS=-Wno-implicit-int -Wno-format
dijkstra_CFLAGS=-Wno-return-type
gsm_CFLAGS=-DSASR -Wno-everything
libjpeg_CFLAGS=-Wno-incompatible-library-redeclaration -Wno-implicit-function-declaration -Wno-shift-negative-value
pgp_CFLAGS=-DUNIX -D_BSD -DPORTABLE -DUSE_NBIO -DMPORTABLE -I${BENCH_DIR}/pgp/ -Wno-everything
rsynth_CFLAGS=-I${BENCH_DIR}/rsynth -Wno-everything -I/usr/local/include
sha_CLFAGS=-Wno-everything
susan_CFLAGS=-Wno-everything

.PHONY: all clean rttests sftests

all: rttests sftests
	@echo "Compilation done!"

sftests: $(BENCHESSF) 

rttests: $(TESTSRT)

clean:
	@rm -rf ${TMP_DIR}
	#@rm -f ${BIN_DIR}/*_wasm.so

%_sf:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_sf=%)"
	${WASMCC} ${$(@:%_sf=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} ${BENCH_DIR}/$(@:%_sf=%)/*.c $(WASM2OBJ_DUMMY) -o ${TMP_DIR}/$(@:%_sf=%).wasm
	${WASM2OBJ_EXECUTABLE} ${TMP_DIR}/$(@:%_sf=%).wasm -o ${TMP_DIR}/$(@:%_sf=%).bc
	${CC} ${CFLAGS} ${OPTFLAGS} -D${USE_MEM} -D${ARCH} ${TMP_DIR}/$(@:%_sf=%).bc ${WASM2OBJ_MEMC} ${WASM2OBJ_RT_LIBC} ${WASM2OBJ_RT_ENV} ${WASM2OBJ_RT_RT} -lm -o ${TMP_DIR}/$(@:%_sf=%)_wasm.out
	${WASM2OBJ_EXECUTABLE} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_sf=%).wasm -o ${TMP_DIR}/$(@:%_sf=%).bc
	${CC} --shared -fPIC ${OPTFLAGS} -I${SERVERLESS_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_sf=%).bc ${SERVERLESS_MEMC} ${SERVERLESS_WASMISA} -lm -o ${TMP_DIR}/$(@:%_sf=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_sf=%)_wasm.so ${SERVERLESS_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

%_rt:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(WASM2OBJ_DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm
	${WASM2OBJ_EXECUTABLE} ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} ${CFLAGS} ${OPTFLAGS} -D${USE_MEM} -D${ARCH} ${TMP_DIR}/$(@:%_rt=%).bc ${WASM2OBJ_MEMC} ${WASM2OBJ_RT_LIBC} ${WASM2OBJ_RT_ENV} ${WASM2OBJ_RT_RT} -lm -o ${TMP_DIR}/$(@:%_rt=%)_wasm.out
	${WASM2OBJ_EXECUTABLE} --inline-constant-globals --runtime-globals ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} --shared -fPIC ${OPTFLAGS} -I${SERVERLESS_RT_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${SERVERLESS_MEMC} ${SERVERLESS_WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${SERVERLESS_BIN_DIR} 
#	@rm -rf ${TMP_DIR}

