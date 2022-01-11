SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

.PHONY: clean
clean: 
	rm -rf res/*

.PHONY: run
run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

.PHONY: debug
debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

.PHONY: client
client:
	curl  -H 'Expect:' -H "Content-Type: text/plain" "${HOSTNAME}:10000"
