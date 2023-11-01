SLEDGE_BINARY_DIR=../../runtime/bin
HOSTNAME=10.10.1.1
PORT1=10050
PORT2=20050
HEY_OPTS=-disable-compression -disable-keepalive -disable-redirects

default: run

clean: 
	rm -rf res/*

run:
	SLEDGE_SIGALRM_HANDLER=TRIAGED SLEDGE_SCHEDULER=MTDBF SLEDGE_SPINLOOP_PAUSE_ENABLED=true SLEDGE_HTTP_SESSION_PERF_LOG=http_perf.log SLEDGE_SANDBOX_PERF_LOG=perf.log LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	SLEDGE_SCHEDULER=MTDBF SLEDGE_SPINLOOP_PAUSE_ENABLED=false SLEDGE_NWORKERS=18 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
		--eval-command="handle SIGUSR1 noprint nostop" \
		--eval-command="handle SIGPIPE noprint nostop" \
		--eval-command="set pagination off" \
		--eval-command="set print pretty" \
		--eval-command="run spec.json"

valgrind:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} valgrind --leak-check=full --max-stackframe=11150456 --run-libc-freeres=no --run-cxx-freeres=no ${SLEDGE_BINARY_DIR}/sledgert spec.json

client-localhost-xyz-once:
	cat ./0_depth.png | http "localhost:${PORT1}/depth_to_xyz" > ./out.png

client-localhost-fib-once:
	http localhost:${PORT2}/fib?30


client-xyz-once:
	cat ./0_depth.png | http "${HOSTNAME}:${PORT1}/depth_to_xyz" > ./out.png

client-xyz-hey:
	hey ${HEY_OPTS} -n 90 -c 90 -t 0 -m POST -D "./0_depth.png" "http://${HOSTNAME}:${PORT1}/depth_to_xyz"

#90=18*5, 4500=1s*72/14400 *90%
client-xyz-loadtest:
	loadtest -n 90 -c 90 -T "image/png" -m POST -b "./0_depth.png" "http://${HOSTNAME}:${PORT1}/depth_to_xyz" 

client-xyz-wrk:
	wrk -s post_binary.lua -t 1 -c 1 -d 1s -R 1 "http://${HOSTNAME}:${PORT1}/depth_to_xyz" -- "0_depth.png"


client-fib-once:
	# echo 30 | http ${HOSTNAME}:${PORT2}/fib
	http ${HOSTNAME}:${PORT2}/fib?30

client-fib-curl:
	curl -i "http://${HOSTNAME}:${PORT2}/fib?30"

#72=18*4, 4500=1s*72/14400 *90%
client-fib-loadtest:
	loadtest -t 10 -c 72 --rps 4500 -P 30 "http://${HOSTNAME}:${PORT2}/fib" 

client-fib-hey:
	hey ${HEY_OPTS} -z 10s -c 72 -t 0 -o csv -m POST -d "30\n" "http://${HOSTNAME}:${PORT2}/fib"

client-fib-wrk:
	wrk -t 1 -c 1 -d 5s -R 1 "http://${HOSTNAME}:${PORT2}/fib?30"

client-admin:
	echo 5 | http ${HOSTNAME}:55555/admin
