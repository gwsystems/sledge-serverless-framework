CURR_DIR= $(shell pwd)
DIST_PREFIX=${CURR_DIR}/dist/

.PHONY: all
all: clean build

.PHONY: build
build: ck jsmn http-parser awsm-wasi

# Concurrency Kit
.PHONY: ck
ck: ck/configure
	mkdir -p ${DIST_PREFIX}
	cd ck; ./configure --cores=$(shell getconf _NPROCESSORS_CONF) --prefix=${DIST_PREFIX}
	make -C ck all
	make -C ck install

# HTTP Parser
${DIST_PREFIX}/include/http_parser.h: http-parser/http_parser.h
	mkdir -p ${DIST_PREFIX}
	mkdir -p ${DIST_PREFIX}/include/
	cp http-parser/http_parser.h ${DIST_PREFIX}/include/

${DIST_PREFIX}/lib/http_parser.o: http-parser/http_parser.c http-parser/http_parser.h
	mkdir -p ${DIST_PREFIX}
	mkdir -p ${DIST_PREFIX}/lib/
	cd http-parser;	$(CC) $(CFLAGS) -I. -c http_parser.c
	mv http-parser/http_parser.o ${DIST_PREFIX}/lib/

.PHONY: http-parser
http-parser: ${DIST_PREFIX}/lib/http_parser.o ${DIST_PREFIX}/include/http_parser.h

# Jasmine JSON Parser
${DIST_PREFIX}/include/jsmn.h: jsmn/jsmn.h
	mkdir -p ${DIST_PREFIX}
	mkdir -p ${DIST_PREFIX}/include/
	cp jsmn/jsmn.h ${DIST_PREFIX}/include/

.PHONY: jsmn
jsmn: ${DIST_PREFIX}/include/jsmn.h

# aWsm WASI
${DIST_PREFIX}/include/wasi_backing.h: ../../awsm/runtime/libc/wasi/include/wasi_backing.h
	@mkdir -p ${DIST_PREFIX}
	@mkdir -p ${DIST_PREFIX}/include/
	@cp ../../awsm/runtime/libc/wasi/include/wasi_backing.h ${DIST_PREFIX}/include/wasi_backing.h

${DIST_PREFIX}/include/wasi_impl.h: ../../awsm/runtime/libc/wasi/include/wasi_impl.h
	@mkdir -p ${DIST_PREFIX}
	@mkdir -p ${DIST_PREFIX}/include/
	@cp ../../awsm/runtime/libc/wasi/include/wasi_impl.h ${DIST_PREFIX}/include/wasi_impl.h

${DIST_PREFIX}/include/wasi_serdes.h: ../../awsm/runtime/libc/wasi/include/wasi_serdes.h
	@mkdir -p ${DIST_PREFIX}
	@mkdir -p ${DIST_PREFIX}/include/
	@cp ../../awsm/runtime/libc/wasi/include/wasi_serdes.h ${DIST_PREFIX}/include/wasi_serdes.h

${DIST_PREFIX}/include/wasi_spec.h: ../../awsm/runtime/libc/wasi/include/wasi_spec.h
	@mkdir -p ${DIST_PREFIX}
	@mkdir -p ${DIST_PREFIX}/include/
	@cp ../../awsm/runtime/libc/wasi/include/wasi_spec.h ${DIST_PREFIX}/include/wasi_spec.h

.PHONY: awsm-wasi
awsm-wasi: ${DIST_PREFIX}/include/wasi_backing.h ${DIST_PREFIX}/include/wasi_impl.h ${DIST_PREFIX}/include/wasi_serdes.h ${DIST_PREFIX}/include/wasi_spec.h

.PHONY: clean
clean:
	@make -C ck uninstall
	@rm -rf ${DIST_PREFIX}

