SLEDGE_BINARY_DIR=../../runtime/bin
HOST?=localhost # pass arguments to change this: make client-lpd HOST=10.10.1.4
# HOST=arena0.andrew.cmu.edu
# HOST=c220g2-011017.wisc.cloudlab.us
PORT0=10000
PORT1=15000
PORT2=20000
PORT3=25000
PORT4=30000
PORT5=35000
PORT6=40000
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


client-cnn:
	curl -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@input-cnn/faces01.jpg" "${HOST}:${PORT0}/cnn"

client-cifar10:
	curl  -H 'Expect: ' -H "Content-Type: image/bmp" --data-binary "@input-cifar10/airplane1.bmp" "${HOST}:${PORT1}/cifar10"

client-gocr:
	curl  -H 'Expect: ' -H "Content-Type: application/octet-stream" --data-binary "@input-gocr/5x8.pnm" "${HOST}:${PORT2}/gocr"

client-lpd:
#	curl -H 'Expect: ' -H "Content-Type: image/png" --data-binary "@input-lpd-png/Cars0.png" "${HOST}:${PORT3}/lpd"
	curl -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@input-lpd-jpg/Cars0.jpg" "${HOST}:${PORT3}/lpd"

client-resize:
	curl -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@input-resize/picsum_512x512_01.jpg" "${HOST}:${PORT4}/resize" --output "out-resize.jpg"

client-ekf:
	curl -H 'Expect: ' -H "Content-Type: application/octet-stream" --data-binary "@input-ekf/iter00.dat" "${HOST}:${PORT5}/ekf" --output "out-ekf-iter00.dat"

client-fib-curl:
	curl -i -H 'Expect: ' -H "Content-Type: text/plain" "${HOST}:${PORT6}/fib?30"

########################################## Choose a random file to send with curl: ##########################################
client-cnn-random:
	@dir="input-cnn"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT0}/cnn"

client-cifar10-random:
	@dir="input-cifar10"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: image/bmp" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT1}/cifar10"

client-gocr-random:
	@dir="input-gocr"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: application/octet-stream" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT2}/gocr"

client-lpd-random:
#	@dir="input-lpd-png"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
#	curl -s -H 'Expect: ' -H "Content-Type: image/png" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT3}/lpd"
	@dir="input-lpd-jpg"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT3}/lpd"

client-resize-random:
	@dir="input-resize"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: image/jpeg" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT4}/resize" --output "out-resize-$$random_file"

client-ekf-random:
	@dir="input-ekf"; random_file="$$(ls $$dir | shuf -n 1)"; echo "Random file: $$random_file"; \
	curl -s -H 'Expect: ' -H "Content-Type: application/octet-stream" --data-binary "@$$dir/$$random_file" "${HOST}:${PORT5}/ekf" --output "out-ekf-$$random_file"
#############################################################################################################################

client-fib-once:
	echo 30 | http ${HOST}:${PORT6}/fib
# http ${HOST}:${PORT6}/fib?30

client-fib-loadtest:
	loadtest -n 10 -c 10 -P 30 "http://${HOST}:${PORT6}/fib" 

client-fib-hey:
	hey ${HEY_OPTS} -z 10s -c 72 -t 0 -o csv -m POST -d "30\n" "http://${HOST}:${PORT6}/fib"

client-fib-wrk:
	wrk -t 1 -c 1 -d 5s -R 1 "http://${HOST}:${PORT6}/fib?30"


client-admin:
	echo 5 | http ${HOST}:55555/admin

client-terminator:
	echo 5 | http ${HOST}:55555/terminator
