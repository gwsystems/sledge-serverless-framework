RUNTIME_DIR=../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
SLEDGE_TESTS_DIR=${RUNTIME_DIR}/tests
HOSTNAME=localhost
DURATION_SEC=15

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

client-fib10-once:
	echo "10" | http :1337

client-fib40-once:
	echo "40" | http :1337

client-preempt:
	(echo "40" | http :10040 &); echo "10" | http :1337

client-fib10-multi:
	hey -z ${DURATION_SEC}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "10\n" "http://${HOSTNAME}:1337"
