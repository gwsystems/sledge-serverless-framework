SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

client:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@hyde.pnm" "${HOSTNAME}:10000"

client2:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../handwriting/handwrt1.pnm" "${HOSTNAME}:10000"

client3:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../fivebyeight/5x8.pnm" "${HOSTNAME}:10000"