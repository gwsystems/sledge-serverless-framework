RUNTIME_DIR=../../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
SLEDGE_TESTS_DIR=${RUNTIME_DIR}/tests
HOSTNAME=localhost
DURATION_SEC=5

all: run

clean: 
	make -C ${RUNTIME_DIR} clean
	make -C ${SLEDGE_TESTS_DIR} clean
	rm -f ${SLEDGE_BINARY_DIR}/fibonacci.wasm.so

${SLEDGE_BINARY_DIR}/sledgert:
	make -C ${RUNTIME_DIR} runtime

.PHONY: sledgert
sledgert: ${SLEDGE_BINARY_DIR}/sledgert

${SLEDGE_BINARY_DIR}/fibonacci.wasm.so:
	make -C ../../../applications fibonacci.install

.PHONY: fibonacci
fibonacci: ${SLEDGE_BINARY_DIR}/fibonacci.wasm.so

run: sledgert fibonacci
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug: sledgert fibonacci
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 \
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="set print pretty" \
		--eval-command="run spec.json"

client-fib30-once:
	echo 30 | http :10031/fib

client-fib40-once:
	http :10030/fib?40

client-fib30-multi:
	hey -z ${DURATION_SEC}s -cpus 2 -c 100 -t 0 -o csv -m POST -d "30\n" "http://${HOSTNAME}:10030/fib"

client-fib40-multi:
	hey -z ${DURATION_SEC}s -cpus 2 -c 100 -t 0 -o csv -m POST -d "40\n" "http://${HOSTNAME}:10030/fib"
