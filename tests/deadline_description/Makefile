SLEDGE_BINARY_DIR=../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	SLEDGE_SCHEDULER=FIFO SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1 \
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

bench-ekf:
	echo 'hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./ekf/initial_state.dat" "http://${HOSTNAME}:10000"' > ekf.txt
	hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./ekf/initial_state.dat" "http://${HOSTNAME}:10000" >> ekf.txt 2>&1

client-ekf:
	cat ./ekf/initial_state.dat | http :10000

bench-resize:
	echo 'hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./resize/shrinking_man_large.jpg" "http://${HOSTNAME}:10001"' > resize.txt
	hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./resize/shrinking_man_large.jpg" "http://${HOSTNAME}:10001" >> resize.txt 2>&1

client-resize:
	cat ./resize/shrinking_man_large.jpg | http "http://${HOSTNAME}:10001"

bench-lpd:
	echo 'hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./lpd/Cars0.png" "http://${HOSTNAME}:10002"' > lpd.txt
	hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./lpd/Cars0.png" "http://${HOSTNAME}:10002" >> lpd.txt 2>&1

client-lpd:
	cat ./lpd/Cars0.png | http "http://${HOSTNAME}:10002"

bench-gocr:
	echo 'hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./gocr/hyde.pnm" "http://${HOSTNAME}:10003"' > gocr.txt
	hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./gocr/hyde.pnm" "http://${HOSTNAME}:10003" >> gocr.txt 2>&1

client-gocr:
	cat ./gocr/hyde.pnm | http "http://${HOSTNAME}:10003"

bench-cifar10:
	echo 'hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./cifar10/airplane1.bmp" "http://${HOSTNAME}:10004"' > cifar10.txt
	hey -disable-compression -disable-keepalive -disable-redirects -z 10m -c 1 -cpus 1 -t 0 -m GET -D "./cifar10/airplane1.bmp" "http://${HOSTNAME}:10004" >> cifar10.txt 2>&1
	
client-cifar10:
	cat ./cifar10/airplane1.bmp | http "http://${HOSTNAME}:10004"


bench-all: bench-ekf bench-resize bench-lpd bench-gocr bench-cifar10

