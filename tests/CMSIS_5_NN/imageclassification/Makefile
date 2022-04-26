SLEDGE_BINARY_DIR=../../../runtime/bin

.PHONY: run
run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

.PHONY: debug
debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert \
	--eval-command="handle SIGUSR1 nostop" \
	--eval-command="handle SIGPIPE nostop" \
	--eval-command="set pagination off" \
	--eval-command="run spec.json"

.PHONY: client
client:
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/airplane1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/automobile1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/bird1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/cat1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/deer1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/dog1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/frog1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/horse1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/ship1.bmp" "localhost:10000/random"
	@curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/truck1.bmp" "localhost:10000/random"
