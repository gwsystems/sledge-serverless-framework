CURR_DIR= $(shell pwd)
DIST_PREFIX=${CURR_DIR}/dist/

all: clean build

build: ck jsmn http-parser

ck:
	mkdir -p ${DIST_PREFIX}
	cd ck; ./configure --cores=$(shell getconf _NPROCESSORS_CONF) --prefix=${DIST_PREFIX}
	make -C ck all
	make -C ck install

http-parser: http-parser/http_parser.c
	mkdir -p ${DIST_PREFIX}/lib/
	cd http-parser;	$(CC) $(CFLAGS) -I. -c http_parser.c; mv http_parser.o ${DIST_PREFIX}/lib/; cp http_parser.h ${DIST_PREFIX}/include/

jsmn:
	mkdir -p ${DIST_PREFIX}/include/
	cp jsmn/jsmn.h ${DIST_PREFIX}/include/

clean:
	make -C ck uninstall
	rm -rf ${DIST_PREFIX}

.PHONY: clean all build ck jsmn http-parser