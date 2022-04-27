CURR_DIR= $(shell pwd)
DIST_PREFIX=${CURR_DIR}/dist/

.PHONY: all
all: clean build

.PHONY: build
build: ck jsmn http-parser

# Concurrency Kit
ck/Makefile: ck/Makefile.in ck/configure
	cd ck; ./configure --cores=$(shell getconf _NPROCESSORS_CONF) --prefix=${DIST_PREFIX}

${DIST_PREFIX}/lib/libck.so: ck/Makefile
	make -C ck all install

.PHONY: ck
ck: ${DIST_PREFIX}/lib/libck.so

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

.PHONY: clean
clean:
	-@test -f ck/Makefile && make -k -C ck uninstall || echo "Makefile not generated. Skipping!"
	-@rm -f ck/Makefile
	-@rm -rf ${DIST_PREFIX}
