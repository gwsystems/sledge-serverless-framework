RUNTIME_DIR=../../../runtime/
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
	curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@initial_state.dat" "${HOSTNAME}:10000" --output result1.dat
	diff -s result1.dat one_iteration.dat
	curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@result1.dat" "${HOSTNAME}:10000" --output result2.dat
	diff -s result2.dat two_iterations.dat
	curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@result2.dat" "${HOSTNAME}:10000" --output result3.dat
	diff -s result3.dat three_iterations.dat
	rm result*.dat
