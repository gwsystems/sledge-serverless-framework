RUNTIME_DIR=../../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
HOSTNAME=localhost

.PHONY: all
all: run

.PHONY: clean
clean: 
	rm -rf res/*

.PHONY: run
run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

.PHONY: debug
debug:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

.PHONY: client
client:
	curl -H 'Expect:' -H "Content-Type: image/jpeg" --data-binary "@cnn_faces.jpg" "${HOSTNAME}:10000/face"
