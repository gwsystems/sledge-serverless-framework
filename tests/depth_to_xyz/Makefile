SLEDGE_BINARY_DIR=../../runtime/bin
HOSTNAME=localhost
PORT=10000
HEY_OPTS=-disable-compression -disable-keepalive -disable-redirects

default: run

clean: 
	rm -rf res/*

run:
	SLEDGE_SCHEDULER=MTDBF LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	SLEDGE_SCHEDULER=MTDBF LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="set print pretty" \
		--eval-command="run spec.json"

valgrind:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} valgrind --leak-check=full --max-stackframe=11150456 --run-libc-freeres=no --run-cxx-freeres=no ${SLEDGE_BINARY_DIR}/sledgert spec.json

client:
	curl -H 'Expect:' -H "Content-Type: image/png" --data-binary "@./0_depth.png" --output "out.png" "${HOSTNAME}:${PORT}/depth_to_xyz"

client-hey:
	hey ${HEY_OPTS} -n 90 -c 90 -t 0 -m POST -D "./0_depth.png" "http://${HOSTNAME}:${PORT}/depth_to_xyz"

client-loadtest:
	loadtest -n 90 -c 90 -T "image/png" -m POST -b "./0_depth.png" "http://${HOSTNAME}:${PORT}/depth_to_xyz" 
