RUNTIME_DIR=../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
SLEDGE_TESTS_DIR=${RUNTIME_DIR}/tests
HOSTNAME=localhost

all: run

clean: 
	make -C ${RUNTIME_DIR} clean
	make -C ${SLEDGE_TESTS_DIR} clean
	rm -f ${SLEDGE_BINARY_DIR}/html.wasm.so

${SLEDGE_BINARY_DIR}/sledgert:
	make -C ${RUNTIME_DIR} runtime

.PHONY: sledgert
sledgert: ${SLEDGE_BINARY_DIR}/sledgert

${SLEDGE_BINARY_DIR}/html.wasm.so:
	make -C ../../applications html.install

.PHONY: html
html: ${SLEDGE_BINARY_DIR}/html.wasm.so

run: sledgert html
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug: sledgert html
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 \
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

client:
	http :1337

browser-args:
	xdg-open "http://localhost:1337"

client-stdin:
	echo "Example STDIN" | http :1337

client-args:
	http ":1337?firstArg&secondArg&thirdArg"

browser-args:
	xdg-open "http://localhost:1337?firstArg&secondArg&thirdArg"

client-stdin-args:
	echo "Example STDIN" | http ":1337?firstArg&secondArg&thirdArg"
