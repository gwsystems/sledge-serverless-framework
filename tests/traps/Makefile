RUNTIME_DIR=../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
SLEDGE_TESTS_DIR=${RUNTIME_DIR}/tests
HOSTNAME=localhost
DURATION_SEC=15

all: run

clean: 
	make -C ${RUNTIME_DIR} clean
	make -C ${SLEDGE_TESTS_DIR} clean
	rm -f ${SLEDGE_BINARY_DIR}/trap_divzero.wasm.so

${SLEDGE_BINARY_DIR}/sledgert:
	make -C ${RUNTIME_DIR} runtime

.PHONY: sledgert
sledgert: ${SLEDGE_BINARY_DIR}/sledgert

${SLEDGE_BINARY_DIR}/trap_divzero.wasm.so:
	make -C ../../applications trap_divzero.install

.PHONY: trap_divzero
trap_divzero: ${SLEDGE_BINARY_DIR}/trap_divzero.wasm.so

run: sledgert trap_divzero
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug: sledgert trap_divzero
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 \
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="handle SIGFPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

client-ok:
	echo "1" | http :10000/divide

client-trap:
	echo "0" | http --timeout 3600 :10000/divide

client-trap2:
	echo "-1" | http :10000/divide

test:
	echo "4" | http :10000/divide
	echo "0" | http :10000/divide
	echo "3" | http :10000/divide
	echo "0" | http :10000/divide
	echo "2" | http :10000/divide
	echo "0" | http :10000/divide
	echo "1" | http :10000/divide
	echo "0" | http :10000/divide
	echo "1" | http :10000/divide
	echo "0" | http :10000/divide
	echo "2" | http :10000/divide
	echo "0" | http :10000/divide
	echo "3" | http :10000/divide
	echo "0" | http :10000/divide
	echo "4" | http :10000/divide

client-fib10-multi:
	hey -z ${DURATION_SEC}s -cpus 4 -c 100 -t 0 -o csv -m POST -d "10\n" "http://${HOSTNAME}:10010/divide"
