#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2155
source ../bash_libraries/multi_tenancy_base.sh || exit 1

# Configure SERVER parameters: (this is to skip the .env config file)
export SLEDGE_SCHEDULER=MTDBF
export SLEDGE_DISABLE_PREEMPTION=false
export SLEDGE_SANDBOX_PERF_LOG=perf.log
# export SLEDGE_HTTP_SESSION_PERF_LOG=http_perf.log
export SLEDGE_NWORKERS=8
# export SLEDGE_PROC_MHZ=2100
export EXTRA_EXEC_PERCENTILE=10

# To reduce post processing time, provide local-only meaningful metrics:
# Comment it in order to use ALL the metrics!
declare -a SANDBOX_METRICS=(total running_sys running_user)

# The global configs for the scripts
declare -r CLIENT_TERMINATE_SERVER=true
declare -r DURATION_sec=30
declare -r ESTIMATIONS_PERCENTILE=60
declare -r NWORKERS=${SLEDGE_NWORKERS:-1}

# Tenant configs:
declare -ar TENANT_IDS=("sod")
declare -ar INIT_PORTS=(25000)
declare -ar ROUTES=("lpd")
declare -ar MTDS_REPL_PERIODS_us=(0)
declare -ar MTDS_MAX_BUDGETS_us=(0)
declare -ar MTDBF_RESERVATIONS_p=(0)

# Per route configs:
declare -ar WASM_PATHS=("$LPD")
declare -ar RESP_CONTENT_TYPES=("text/plain")
declare -ar EXPECTED_EXEC_TIMES_us=("16700")
declare -ar DEADLINE_TO_EXEC_RATIOs=("500") # percentage

# For HEY -d is text, -D is file input. For LoadTest -P is text, -b is file input.
# ALso, LoadTest now supports -B for random file in the folder. HEY supports a single file.
# declare -ar ARG_OPTS_HEY=("-D" "-D" "-D" "-D -D")
declare -ar ARG_OPTS_LT=("-B")
declare -ar ARGS=("input-lpd-jpg")

# This is needed if you want loadtest to log the randomly chosen filenames
declare -gr LOADTEST_LOG_RANDOM=false

# 100=FULL load, 50=HALF load ...
declare -ar LOADS=("100")

# When trying varying values, you must set ONE value from the above params to ? (question mark)
# For example, for varying the loads, try: LOADS=("50 ?" "100")
# declare -ar VARYING=(0) # no variation, single experiment
declare -ar VARYING=(0)

run_init
generate_spec_json
framework_init "$@"
