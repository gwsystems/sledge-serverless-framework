#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2155
source ../bash_libraries/multi_tenancy_base.sh || exit 1

# Configure SERVER parameters: (this is to skip the .env config file)
export SLEDGE_SCHEDULER=MTDBF
export SLEDGE_DISABLE_PREEMPTION=false
# export SLEDGE_SANDBOX_PERF_LOG=perf.log
# export SLEDGE_HTTP_SESSION_PERF_LOG=http_perf.log
export SLEDGE_NWORKERS=18
export SLEDGE_PROC_MHZ=2660
export EXTRA_EXEC_PERCENTILE=10

# To reduce post processing time, provide local-only meaningful metrics:
# Comment it in order to use ALL the metrics!
declare -a SANDBOX_METRICS=(total running_sys running_user)

# The global configs for the scripts
# declare -r CLIENT_TERMINATE_SERVER=true
declare -r DURATION_sec=30
declare -r ESTIMATIONS_PERCENTILE=60
declare -r NWORKERS=${SLEDGE_NWORKERS:-1}

# Tenant configs:
declare -ar TENANT_IDS=("fib1" "fib2")
declare -ar INIT_PORTS=(40000 50000)
declare -ar ROUTES=("fib" "fib")
declare -ar MTDS_REPL_PERIODS_us=(0 0)
declare -ar MTDS_MAX_BUDGETS_us=(0 0)
declare -ar MTDBF_RESERVATIONS_p=(50 50)

# Per route configs:
declare -ar WASM_PATHS=("$FIBONACCI" "$FIBONACCI")
declare -ar RESP_CONTENT_TYPES=("text/plain" "text/plain")
declare -ar EXPECTED_EXEC_TIMES_us=("5000" "5000") # cloudlab server
declare -ar DEADLINE_TO_EXEC_RATIOs=("500" "500") # percentage

# For HEY -d is text, -D is file input. For LoadTest -P is text, -b is file input.
# ALso, LoadTest now supports -B for random file in the folder. HEY supports a single file.
declare -ar ARG_OPTS_HEY=("-d" "-d")
declare -ar ARG_OPTS_LT=("-P" "-P")
declare -ar ARGS=("30" "30")

# This is needed if you want loadtest to log the randomly chosen filenames
declare -gr LOADTEST_LOG_RANDOM=false

# 100=FULL load, 50=HALF load ...
declare -ar LOADS=("?" "100") # 4.4 for lab server

# When trying varying values, you must set ONE value from the above params to ? (question mark)
# For example, for varying the loads, try: LOADS=("50 ?" "100")
# declare -ar VARYING=(0) # no variation, single experiment
declare -ar VARYING=(10 20 30 40 50 60 70 80 90 100)

run_init "$@"
generate_spec_json
framework_init "$@"

# Lab server 2100 MHz run user metrics:
# fib(30):  5500
# fib(36):  98000
# cifar10:  5400
# gocr:     11800
# lpd:      20700
# resize:   54000
# ekf:      20

# Cloudlab server 2660 MHz run user metrics:
# fib(30):  5500
# fib(36):  98000
# cifar10:  4250
# gocr:     9150
# lpd:      17800
# resize:   65000
# ekf:      20

# Emil PC (WSL) run user metrics:
# fib(30): 3000
# fib(36): 53000
# cifar10: ?
# resize_small: ?