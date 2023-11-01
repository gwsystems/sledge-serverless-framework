# shellcheck shell=bash
# shellcheck disable=SC2034,SC2153,SC2154,SC2155
if [ -n "$__experiment_server_globals_sh__" ]; then return; fi
__experiment_server_globals_sh__=$(date)

# The global configs for the scripts
declare -gr SERVER_LOG_FILE="perf.log"
declare -gr SERVER_HTTP_LOG_FILE="http_perf.log"
declare -gr HEY_OPTS="-disable-compression -disable-keepalive -disable-redirects"

# Globals to fill during run_init in run.sh, to use in base and generate_spec
declare -A ports=()
declare -A repl_periods=()
declare -A max_budgets=()
declare -A wasm_paths=()
declare -A expected_execs=()
declare -A deadlines=()
declare -A resp_content_types=()
declare -A arg_opts_hey=()
declare -A arg_opts_lt=()
declare -A args=()
declare -A concurrencies=()
declare -A rpss=()
declare -a workloads=()
declare -A workload_tids=()
declare -A workload_deadlines=()
declare -A workload_vars=()

# Sandbox Perf Log Globals:
declare  -ga SANDBOX_METRICS=(total queued uninitialized allocated initialized runnable interrupted preempted running_sys running_user asleep returned complete error)
declare  -gA SANDBOX_METRICS_FIELDS=(
	[total]=6
	[queued]=7
	[uninitialized]=8
	[allocated]=9
	[initialized]=10
	[runnable]=11
	[interrupted]=12
	[preempted]=13
	[running_sys]=14
	[running_user]=15
	[asleep]=16
	[returned]=17
	[complete]=18
	[error]=19
)
declare -gr SANDBOX_TENANT_NAME_FIELD=2
declare -gr SANDBOX_ROUTE_FIELD=3
declare -gr SANDBOX_CPU_FREQ_FIELD=20
declare -gr SANDBOX_RESPONSE_CODE_FIELD=21

# HTTP Session Perf Log Globals:
declare  -ga HTTP_METRICS=(http_receive http_sent http_total)
declare  -gA HTTP_METRICS_FIELDS=(
	[http_receive]=6
	[http_sent]=7
	[http_total]=8
)
declare -gr HTTP_TENANT_NAME_FIELD=1
declare -gr HTTP_ROUTE_FIELD=2
declare -gr HTTP_CPU_FREQ_FIELD=9

assert_run_experiments_args() {
	if (($# != 3)); then
		panic "invalid number of arguments \"$#\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	elif [[ ! -d "$2" ]]; then
		panic "directory \"$2\" does not exist"
		return 1
	elif [[ -z "$3" ]]; then
		panic "load gen \"$3\" was empty"
		return 1
	fi
}

assert_process_client_results_args() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi
}

assert_process_server_results_args() {
	if (($# != 1)); then
		panic "invalid number of arguments \"$#\""
		return 1
	elif [[ ! -d "$1" ]]; then
		panic "directory \"$1\" does not exist"
		return 1
	fi
}

load_value() {
	local  result=$1
	if [ "$result" = "?" ]; then
		result=$2
	fi
    echo "$result"
}

run_init() {
	for var in "${VARYING[@]}"; do
		for t_idx in "${!TENANT_IDS[@]}"; do
			local tenant_id=${TENANT_IDS[$t_idx]}
			local tenant=$(printf "%s-%03d" "$tenant_id" "$var")
			local port=$((INIT_PORTS[t_idx]+var))
			local repl_period=$(load_value ${MTDS_REPL_PERIODS_us[$t_idx]} $var)
			local budget=$(load_value ${MTDS_MAX_BUDGETS_us[$t_idx]} $var)

			# TENANTS+=("$tenant")
			ports+=([$tenant]=$port)
			repl_periods+=([$tenant]=$repl_period)
			max_budgets+=([$tenant]=$budget)

			local t_routes r_expected_execs r_deadlines r_arg_opts_hey r_arg_opts_lt r_args r_loads

			IFS=' ' read -r -a t_routes <<< "${ROUTES[$t_idx]}"
			IFS=' ' read -r -a r_wasm_paths <<< "${WASM_PATHS[$t_idx]}"
			IFS=' ' read -r -a r_expected_execs <<< "${EXPECTED_EXEC_TIMES_us[$t_idx]}"
			IFS=' ' read -r -a r_deadlines <<< "${DEADLINES_us[$t_idx]}"
			IFS=' ' read -r -a r_resp_content_types <<< "${RESP_CONTENT_TYPES[$t_idx]}"

			IFS=' ' read -r -a r_arg_opts_hey <<< "${ARG_OPTS_HEY[$t_idx]}"
			IFS=' ' read -r -a r_arg_opts_lt <<< "${ARG_OPTS_LT[$t_idx]}"
			IFS=' ' read -r -a r_args <<< "${ARGS[$t_idx]}"
			IFS=' ' read -r -a r_loads <<< "${LOADS[$t_idx]}"

			for r_idx in "${!t_routes[@]}"; do
				local route=${t_routes[$r_idx]}
				local wasm_path=${r_wasm_paths[$r_idx]}
				local expected=${r_expected_execs[$r_idx]}
				local deadline=${r_deadlines[$r_idx]}
				local resp_content_type=${r_resp_content_types[$r_idx]}
				local arg_opt_hey=${r_arg_opts_hey[$r_idx]}
				local arg_opt_lt=${r_arg_opts_lt[$r_idx]}
				local arg=${r_args[$r_idx]}
				local load=$(load_value ${r_loads[$r_idx]} $var)

				local workload="$tenant-$route"

				# Divide as float, cast the result to int (Loadtest is okay floats, HEY is not)
				local con=$(echo "x = $NWORKERS * $deadline / $expected * $load / 100; x/1" | bc)
				local rps=$((1000000 * con / deadline))
				# local rps=$(echo "x = 1000000 * $con / $deadline; x/1" | bc)

				wasm_paths+=([$workload]=$wasm_path)
				expected_execs+=([$workload]=$expected)
				deadlines+=([$workload]=$deadline)
				resp_content_types+=([$workload]=$resp_content_type)
				arg_opts_hey+=([$workload]=$arg_opt_hey)
				arg_opts_lt+=([$workload]=$arg_opt_lt)
				args+=([$workload]=$arg)
				concurrencies+=([$workload]=$con)
				rpss+=([$workload]=$rps)
				workloads+=("$workload")
				workload_tids+=([$workload]=$tenant_id)
				workload_deadlines+=([$workload]=$deadline)
				workload_vars+=([$workload]=$var)
			done
		done
	done
}
