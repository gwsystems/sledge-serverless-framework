SLEDGE_BINARY_DIR=../../runtime/bin
HOSTNAME=localhost
PORT=10000

default: run

clean: 
	rm -rf res/*

run:
	SLEDGE_SPINLOOP_PAUSE_ENABLED=true SLEDGE_SANDBOX_PERF_LOG=perf.log LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	SLEDGE_SPINLOOP_PAUSE_ENABLED=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

valgrind:
	SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} valgrind --leak-check=full --max-stackframe=11150456 --run-libc-freeres=no --run-cxx-freeres=no ${SLEDGE_BINARY_DIR}/sledgert spec.json

.PHONY: client
client:
	cat ./0_depth.png | http "${HOSTNAME}:${PORT}/depth_to_xyz" > ./out.png

client-cloudlab:
	cat ./0_depth.png | http "c220g2-011017.wisc.cloudlab.us:${PORT}/depth_to_xyz" > ./out.png

client-cmu:
	cat ./0_depth.png | http "arena0.andrew.cmu.edu:${PORT}/depth_to_xyz" > ./out.png

multi:
	hey -n 180 -c 180 -t 0 -o csv -m POST -D "./0_depth.png" "http://${HOSTNAME}:${PORT}/depth_to_xyz"
