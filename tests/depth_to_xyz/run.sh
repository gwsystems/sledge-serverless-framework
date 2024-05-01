#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2155
source ../bash_libraries/multi_tenancy_base.sh || exit 1

# Configure SERVER parameters: (this is to skip the .env config file)
export SLEDGE_SCHEDULER=MTDBF
export SLEDGE_DISABLE_PREEMPTION=false
export SLEDGE_SANDBOX_PERF_LOG=perf.log
export SLEDGE_HTTP_SESSION_PERF_LOG=http_perf.log
# export SLEDGE_NWORKERS=1
# export SLEDGE_PROC_MHZ=2100
# export EXTRA_EXEC_PERCENTILE=10

# The global configs for the scripts
declare -r CLIENT_TERMINATE_SERVER=false
declare -r DURATION_sec=30
declare -r ESTIMATIONS_PERCENTILE=60
declare -r NWORKERS=${SLEDGE_NWORKERS:-1}

# Tenant configs:
declare -ar TENANT_IDS=("cmu")
declare -ar INIT_PORTS=(10000)
declare -ar ROUTES=("depth_to_xyz")
declare -ar MTDS_REPL_PERIODS_us=(0)
declare -ar MTDS_MAX_BUDGETS_us=(0)
declare -ar MTDBF_RESERVATIONS_p=(0)

# Per route configs:
declare -ar WASM_PATHS=("depth_to_xyz.wasm.so")
declare -ar RESP_CONTENT_TYPES=("image/png")
declare -ar EXPECTED_EXEC_TIMES_us=("950000")
declare -ir DEADLINE_TO_EXEC_RATIO=500 # percentage

# For HEY -d is text, -D is file input. For LoadTest -P is text, -b is file input.
declare -ar ARG_OPTS_HEY=("-D")
declare -ar ARG_OPTS_LT=("-b")
declare -ar ARGS=("./0_depth.png")

# 100=FULL load, 50=HALF load ...
declare -ar LOADS=("100")

# When trying varying values, you must set ONE value from the above params to ? (question mark)
# For example, for varying the loads, try: LOADS=("50 ?" "100")
# declare -ar VARYING=(0) # no variation, single experiment
declare -ar VARYING=(0)

run_init
generate_spec_json
framework_init "$@"
