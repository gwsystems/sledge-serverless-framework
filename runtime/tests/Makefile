include Makefile.inc

BENCH_DIR=../../silverfish/code_benches/

TESTS=fibonacci #empty work
#TESTS=forever filesys sockserver sockclient empty
TESTSRT=$(TESTS:%=%_rt)
BENCHES=adpcm basic_math binarytrees bitcount blowfish crc dijkstra fft function_pointers \
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
	${WASMCC} ${$(@:%_sf=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} ${BENCH_DIR}/$(@:%_sf=%)/*.c $(DUMMY) -o ${TMP_DIR}/$(@:%_sf=%).wasm
	${SFCC} ${TMP_DIR}/$(@:%_sf=%).wasm -o ${TMP_DIR}/$(@:%_sf=%).bc
	${CC} ${CFLAGS} ${OPTFLAGS} -D${USE_MEM} ${TMP_DIR}/$(@:%_sf=%).bc ${MEMC} ${RT_LIBC} ${RT_RT} -o ${TMP_DIR}/$(@:%_sf=%)_wasm.out
	${CC} --shared -fPIC ${OPTFLAGS} -I${ART_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_sf=%).bc ${AMEMC} ${WASMISA} -o ${TMP_DIR}/$(@:%_sf=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_sf=%)_wasm.so ${ABIN_DIR} 
#	@rm -rf ${TMP_DIR}

%_rt:
	@mkdir -p ${TMP_DIR}
	@echo "Compiling $(@:%_rt=%)"
	${WASMCC} ${$(@:%_rt=%)_CFLAGS} ${WASMCFLAGS} ${OPTFLAGS} $(@:%_rt=%)/*.c $(DUMMY) -o ${TMP_DIR}/$(@:%_rt=%).wasm
	${SFCC} ${TMP_DIR}/$(@:%_rt=%).wasm -o ${TMP_DIR}/$(@:%_rt=%).bc
	${CC} ${CFLAGS} ${OPTFLAGS} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${MEMC} ${RT_LIBC} ${RT_RT} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.out
	${CC} --shared -fPIC ${OPTFLAGS} -I${ART_INC} -D${USE_MEM} ${TMP_DIR}/$(@:%_rt=%).bc ${AMEMC} ${WASMISA} -o ${TMP_DIR}/$(@:%_rt=%)_wasm.so
	@cp ${TMP_DIR}/$(@:%_rt=%)_wasm.so ${ABIN_DIR} 
#	@rm -rf ${TMP_DIR}

