RUNTIME_DIR=../../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
SLEDGE_TESTS_DIR=${RUNTIME_DIR}/tests
HOSTNAME=localhost
DURATION_SEC=15

all: run

clean: 
	make -C ${RUNTIME_DIR} clean
	make -C ${SLEDGE_TESTS_DIR} clean
	rm -f ${SLEDGE_BINARY_DIR}/echo.wasm.so

${SLEDGE_BINARY_DIR}/sledgert:
	make -C ${RUNTIME_DIR} runtime

.PHONY: sledgert
sledgert: ${SLEDGE_BINARY_DIR}/sledgert

${SLEDGE_BINARY_DIR}/echo.wasm.so:
	make -C ../../../applications echo.install

.PHONY: echo
echo: ${SLEDGE_BINARY_DIR}/echo.wasm.so

run: sledgert echo
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug: sledgert echo
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 \
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

client-echo-once:
	http :10050/echo?John

client-preempt:
	(http :10050/echo?John &); http :10050/echo?Doe
