CURR_DIR= $(shell pwd)
DIST_PREFIX=${CURR_DIR}/dist/

.PHONY: all
all: clean build

.PHONY: build
build: ck jsmn llhttp

# Concurrency Kit
ck/Makefile: ck/Makefile.in ck/configure
	cd ck; ./configure --cores=$(shell getconf _NPROCESSORS_CONF) --prefix=${DIST_PREFIX}

${DIST_PREFIX}/lib/libck.so: ck/Makefile
	make -C ck all install

.PHONY: ck
ck: ${DIST_PREFIX}/lib/libck.so

# llhttp HTTP Parser (pre-generated C release of nodejs/llhttp, vendored in llhttp/)
# We compile the release's three C sources directly so the build needs no Node/npm/llparse toolchain.
LLHTTP_CFLAGS = -O3 -Illhttp/include

${DIST_PREFIX}/include/llhttp.h: llhttp/include/llhttp.h
	mkdir -p ${DIST_PREFIX}/include/
	cp llhttp/include/llhttp.h ${DIST_PREFIX}/include/

${DIST_PREFIX}/lib/llhttp.o: llhttp/src/llhttp.c llhttp/include/llhttp.h
	mkdir -p ${DIST_PREFIX}/lib/
	$(CC) $(CFLAGS) $(LLHTTP_CFLAGS) -c llhttp/src/llhttp.c -o $@

${DIST_PREFIX}/lib/llhttp_api.o: llhttp/src/api.c llhttp/include/llhttp.h
	mkdir -p ${DIST_PREFIX}/lib/
	$(CC) $(CFLAGS) $(LLHTTP_CFLAGS) -c llhttp/src/api.c -o $@

${DIST_PREFIX}/lib/llhttp_http.o: llhttp/src/http.c llhttp/include/llhttp.h
	mkdir -p ${DIST_PREFIX}/lib/
	$(CC) $(CFLAGS) $(LLHTTP_CFLAGS) -c llhttp/src/http.c -o $@

.PHONY: llhttp
llhttp: ${DIST_PREFIX}/lib/llhttp.o ${DIST_PREFIX}/lib/llhttp_api.o ${DIST_PREFIX}/lib/llhttp_http.o ${DIST_PREFIX}/include/llhttp.h

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
