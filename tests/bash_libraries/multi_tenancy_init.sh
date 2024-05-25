# shellcheck shell=bash
# shellcheck disable=SC2034,SC2153,SC2154,SC2155
if [ -n "$__run_init_sh__" ]; then return; fi
__run_init_sh__=$(date)

# Globals to fill during run_init in run.sh, to use in base and generate_spec
declare -A ports=()
declare -A repl_periods=()
declare -A max_budgets=()
declare -A reservations=()
declare -A wasm_paths=()
declare -A preprocess_wasm_paths=()
declare -A expected_execs=()
declare -A deadlines=()
declare -A resp_content_types=()
declare -A model_biases=()
declare -A model_scales=()
declare -A model_num_of_params=()
declare -A model_beta1s=()
declare -A model_beta2s=()
declare -A arg_opts_hey=()
declare -A arg_opts_lt=()
declare -A args=()
declare -A loads=()
declare -a workloads=()
declare -A workload_tids=()
declare -A workload_vars=()

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
			[ "$NUCLIO_MODE_ENABLED" == true ] && port=${INIT_PORTS[t_idx]}
			local repl_period=$(load_value "${MTDS_REPL_PERIODS_us[$t_idx]}" "$var")
			local budget=$(load_value "${MTDS_MAX_BUDGETS_us[$t_idx]}" "$var")
			local reservation=$(load_value "${MTDBF_RESERVATIONS_p[$t_idx]}" "$var")

			ports+=([$tenant]=$port)
			repl_periods+=([$tenant]=$repl_period)
			max_budgets+=([$tenant]=$budget)
			reservations+=([$tenant]=$reservation)

			local t_routes r_expected_execs r_deadlines r_arg_opts_hey r_arg_opts_lt r_args r_loads
			local r_model_biases r_model_scales r_model_num_of_params r_model_beta1s r_model_beta2s

			IFS=' ' read -r -a t_routes <<< "${ROUTES[$t_idx]}"
			IFS=' ' read -r -a r_wasm_paths <<< "${WASM_PATHS[$t_idx]}"
			IFS=' ' read -r -a r_expected_execs <<< "${EXPECTED_EXEC_TIMES_us[$t_idx]}"
			IFS=' ' read -r -a r_dl_to_exec_ratios <<< "${DEADLINE_TO_EXEC_RATIOs[$t_idx]}"
			IFS=' ' read -r -a r_resp_content_types <<< "${RESP_CONTENT_TYPES[$t_idx]}"

			IFS=' ' read -r -a r_preprocess_wasm_paths <<< "${PREPROCESS_WASM_PATHS[$t_idx]}"
			IFS=' ' read -r -a r_model_biases <<< "${MODEL_BIASES[$t_idx]}"
			IFS=' ' read -r -a r_model_scales <<< "${MODEL_SCALES[$t_idx]}"
			IFS=' ' read -r -a r_model_num_of_params <<< "${MODEL_NUM_OF_PARAMS[$t_idx]}"
			IFS=' ' read -r -a r_model_beta1s <<< "${MODEL_BETA1S[$t_idx]}"
			IFS=' ' read -r -a r_model_beta2s <<< "${MODEL_BETA2S[$t_idx]}"

			IFS=' ' read -r -a r_arg_opts_hey <<< "${ARG_OPTS_HEY[$t_idx]}"
			IFS=' ' read -r -a r_arg_opts_lt <<< "${ARG_OPTS_LT[$t_idx]}"
			IFS=' ' read -r -a r_args <<< "${ARGS[$t_idx]}"
			IFS=' ' read -r -a r_loads <<< "${LOADS[$t_idx]}"

			for r_idx in "${!t_routes[@]}"; do
				local route=${t_routes[$r_idx]}
				local wasm_path=${r_wasm_paths[$r_idx]}
				local expected=$(load_value "${r_expected_execs[$r_idx]}" "$var")
				local dl_to_exec_ratio=${r_dl_to_exec_ratios[$r_idx]}
				local deadline=$((expected*dl_to_exec_ratio/100))
				local resp_content_type=${r_resp_content_types[$r_idx]}
				local preprocess_wasm_path=${r_preprocess_wasm_paths[$r_idx]}
				local model_bias=${r_model_biases[$r_idx]}
				local model_scale=${r_model_scales[$r_idx]}
				local model_num_of_param=${r_model_num_of_params[$r_idx]}
				local model_beta1=${r_model_beta1s[$r_idx]}
				local model_beta2=${r_model_beta2s[$r_idx]}
				local arg_opt_hey=${r_arg_opts_hey[$r_idx]}
				local arg_opt_lt=${r_arg_opts_lt[$r_idx]}
				local arg=$(load_value "${r_args[$r_idx]}" "$var")
				local load=$(load_value "${r_loads[$r_idx]}" "$var")
				local workload="$tenant-$route"

				wasm_paths+=([$workload]=$wasm_path)
				expected_execs+=([$workload]=$expected)
				deadlines+=([$workload]=$deadline)
				resp_content_types+=([$workload]=$resp_content_type)
				preprocess_wasm_paths+=([$workload]=$preprocess_wasm_path)
				model_biases+=([$workload]=$model_bias)
				model_scales+=([$workload]=$model_scale)
				model_num_of_params+=([$workload]=$model_num_of_param)
				model_beta1s+=([$workload]=$model_beta1)
				model_beta2s+=([$workload]=$model_beta2)
				arg_opts_hey+=([$workload]=$arg_opt_hey)
				arg_opts_lt+=([$workload]=$arg_opt_lt)
				args+=([$workload]=$arg)
				loads+=([$workload]=$load)
				workloads+=("$workload")
				workload_tids+=([$workload]=$tenant_id)
				workload_vars+=([$workload]=$var)
			done
		done
	done
}
