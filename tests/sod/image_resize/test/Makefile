RUNTIME_DIR=../../../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin
HOSTNAME=localhost

all: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

client:
	curl -H 'Expect:' -H "Content-Type: image/jpeg" --data-binary "@flower.jpg" --output "result.jpg" "${HOSTNAME}:10000/resize"
