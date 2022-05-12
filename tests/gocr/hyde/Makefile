SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

valgrind:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} valgrind --leak-check=full --max-stackframe=11150456 --run-libc-freeres=no --run-cxx-freeres=no ${SLEDGE_BINARY_DIR}/sledgert spec.json

client:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@hyde.pnm" "${HOSTNAME}:10000/gocr"

client2:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../handwriting/handwrt1.pnm" "${HOSTNAME}:10000/gocr"

client3:
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../fivebyeight/5x8.pnm" "${HOSTNAME}:10000/gocr"
