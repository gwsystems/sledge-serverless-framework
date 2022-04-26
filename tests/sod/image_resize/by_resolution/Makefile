RUNTIME_DIR=../../../../runtime/
SLEDGE_BINARY_DIR=${RUNTIME_DIR}/bin

all: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="run spec.json"

client-small:
	cat shrinking_man_small.jpg | http --output res/small.jpg --download :10000/resize_small

client-medium:
	cat shrinking_man_medium.jpg | http --output res/medium.jpg --download :10000/resize_medium

client-large:
	cat shrinking_man_large.jpg | http --output res/large.jpg --download :10000/resize_large

client: client-small client-medium client-large
