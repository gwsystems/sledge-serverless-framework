#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2155
source ../bash_libraries/multi_tenancy_base.sh || exit 1

# Configure SERVER parameters: (this is to skip the .env config file)
export SLEDGE_SCHEDULER=SJF
export SLEDGE_DISABLE_PREEMPTION=false
export SLEDGE_SPINLOOP_PAUSE_ENABLED=false
export SLEDGE_SANDBOX_PERF_LOG=perf.log
export SLEDGE_HTTP_SESSION_PERF_LOG=http_perf.log
export SLEDGE_NWORKERS=1
# export SLEDGE_PROC_MHZ=2660

# To reduce post processing time, provide local-only meaningful metrics:
# Comment it in order to use ALL the metrics!
declare -a SANDBOX_METRICS=(total running_sys running_user)

# The global configs for the scripts
declare -r ADMIN_ACCESS=false
declare -r CLIENT_TERMINATE_SERVER=false
declare -r DURATION_sec=60
declare -r ESTIMATIONS_PERCENTILE=60
declare -r NWORKERS=${SLEDGE_NWORKERS:-1}

# Tenant configs:
declare -ar TENANT_IDS=("cnn" "cifar10" "gocr" "lpd" "resize" "ekf")
declare -ar INIT_PORTS=(10000 15000 20000 25000 30000 35000)
declare -ar ROUTES=("cnn" "cifar10" "gocr" "lpd" "resize" "ekf")

declare -r NONE="0"
declare -r GET_JPEG_RESOLUTION="get_jpeg_resolution.wasm.so"

# Per route configs:
declare -ar WASM_PATHS=("$CNN" "$CIFAR10" "$GOCR" "$LPD" "$RESIZE" "$EKF")
declare -ar WASM_PATHS_PREPROCESSING=("$GET_JPEG_RESOLUTION" "$GET_JPEG_RESOLUTION" "$GET_JPEG_RESOLUTION" "$GET_JPEG_RESOLUTION" "$NONE")
declare -ar RESP_CONTENT_TYPES=("text/plain" "text/plain" "text/plain" "text/plain" "image/jpeg" "application/octet-stream")
declare -ar EXPECTED_EXEC_TIMES_us=("600000" "4000" "8900" "16700" "62000" "30")
declare -ar DEADLINE_TO_EXEC_RATIOs=("500" "500" "500" "500" "500" "5000") # percentage

# Regressions Model input:
declare -ar PREPROCESS_WASM_PATHS=("$GET_JPEG_RESOLUTION" "$NONE" "$NONE"  "$GET_JPEG_RESOLUTION" "$GET_JPEG_RESOLUTION" "$NONE")
declare -ar MODEL_BIASES=("1500" "2000" "2500" "3000" "3500" "100")
declare -ar MODEL_SCALES=("1500" "2000" "2500" "3000" "3500" "100")
declare -ar MODEL_NUM_OF_PARAMS=("2" "1" "1" "2" "2" "1") 
declare -ar MODEL_BETA1S=("1500" "2000" "2500" "3000" "3500" "100")
declare -ar MODEL_BETA2S=("1500" "0" "0" "3000" "3500" "0")

# This is needed if you want loadtest to time out for requests that miss their deadlines (timeout = deadline):
declare -gr LOADTEST_REQUEST_TIMEOUT=false

# For HEY -d is text, -D is file input. For LoadTest -P is text, -b is file input.
# ALso, LoadTest now supports -B for random file in the folder. HEY supports a single file.
declare -ar ARG_OPTS_HEY=("-D" "-D" "-D" "-D" "-D")
declare -ar ARG_OPTS_LT=("-B" "-B" "-B" "-B" "-B") # "-P -P -P")
declare -ar ARGS=("input-cnn" "input-cifar10" "input-gocr" "input-lpd-jpg" "input-resize" "input-ekf")
# declare -ar ARGS=("input-cnn/faces01.jpg" "input-cifar10/airplane1.bmp" "input-gocr/5x8.pnm" "input-lpd-jpg/Cars0.jpg" "input-resize/picsum_512x512_01.jpg" "input-ekf/iter00.dat")

# This is needed if you want loadtest to log the randomly chosen filenames
declare -gr LOADTEST_LOG_RANDOM=false

# 100=FULL load, 50=HALF load ...
declare -ar LOADS=(20 20 25 10 25 1)

# When trying varying values, you must set ONE value from the above params to ? (question mark)
# For example, for varying the loads, try: LOADS=("50 ?" "100")
declare -ar VARYING=(0) # no variation, single experiment

# Add the word "nuclio" to the end of the client execution command to run for Nuclio mode (stick to the same port and use keep-alive)
[[ "${!#}" = "nuclio" || "${!#}" = "Nuclio" ]] && NUCLIO_MODE_ENABLED=true

run_init
generate_spec_json
framework_init "$@"
