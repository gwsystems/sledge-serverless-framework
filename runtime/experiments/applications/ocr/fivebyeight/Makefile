SLEDGE_BINARY_DIR=../../../../bin
SLEDGE_TESTS_DIR=../../../../tests
HOSTNAME=localhost

default: run

clean: 
	make -C ../../../../ clean
	make -C ${SLEDGE_TESTS_DIR} clean
	rm -f ${SLEDGE_BINARY_DIR}/gocr.so

${SLEDGE_BINARY_DIR}/sledgert:
	make -C ../../../../ runtime

${SLEDGE_BINARY_DIR}/so/gocr.so:
	make -C ${SLEDGE_TESTS_DIR} so/gocr.so
	cp ${SLEDGE_TESTS_DIR}/so/gocr.so ${SLEDGE_BINARY_DIR}

run: ${SLEDGE_BINARY_DIR}/sledgert ${SLEDGE_BINARY_DIR}/so/gocr.so
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug: ${SLEDGE_BINARY_DIR}/sledgert ${SLEDGE_BINARY_DIR}/so/gocr.so
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

client:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@5x8.pnm" "${HOSTNAME}:10000"
