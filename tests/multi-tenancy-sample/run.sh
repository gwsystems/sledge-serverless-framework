#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2155

source ../bash_libraries/multi_tenancy_base.sh || exit 1

# To reduce post processing time, provide local-only meaningful metrics:
# Comment the following line in order to use ALL the metrics!
declare -a SANDBOX_METRICS=(total running_sys running_user)

# declare -r APP_WASM="sample_app.wasm.so"
declare -r FIBONACCI_WASM="fibonacci.wasm.so"

# The global configs for the scripts
declare -r CLIENT_TERMINATE_SERVER=true
declare -r DURATION_sec=30
declare -r ESTIMATIONS_PERCENTILE=60
declare -r NWORKERS=$(($(nproc)-2)) # all cores - 2

# Tenant configs:
declare -ar TENANT_IDS=("long" "short")
declare -ar INIT_PORTS=(10000 20000)
# declare -ar ROUTES=("sample_app_route1 sample_app_route2" "fib")
declare -ar ROUTES=("fib1 fib2" "fib")
declare -ar MTDS_REPL_PERIODS_us=(0 0)
declare -ar MTDS_MAX_BUDGETS_us=(0 0)

# Per route configs:
declare -ar WASM_PATHS=("$FIBONACCI_WASM $FIBONACCI_WASM" "$FIBONACCI_WASM")
declare -ar RESP_CONTENT_TYPES=("text/plain text/plain" "text/plain") # image data: "image/png"
declare -ar EXPECTED_EXEC_TIMES_us=("64500 3600" "3600")
declare -ar DEADLINES_us=("322500 18000" "18000")

# For image data: 
# declare -ar ARG_OPTS_HEY=("-D" "-d")
# declare -ar ARG_OPTS_LT=("-b" "-P")
# declare -ar ARGS=("./0_depth.png" "30")

declare -ar ARG_OPTS_HEY=("-d -d" "-d")
declare -ar ARG_OPTS_LT=("-P -P" "-P")
declare -ar ARGS=("36 30" "30")

# 100=FULL, 50=HALF etc.
declare -ar LOADS=("50 70" "100")

# When trying varying values, you must pick ONE value from the above params to ? (question mark)
# For example, for varying the reservations, try: declare -ar LOADS=("50 ?" "100")
declare -ar VARYING=(0) # no variation, single experiment
# declare -ar VARYING=(5 50 100)

run_init
generate_spec_json
# framework_init "$@"
