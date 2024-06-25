# shellcheck shell=bash
# shellcheck disable=SC2154,SC2155
if [ -n "$__generate_spec_json_sh__" ]; then return; fi
__generate_spec_json_sh__=$(date)

jq_admin_spec() {
	jq ". + {\
		\"name\": \"Admin\",\
		\"port\": 55555,\
		\"routes\": [\
			.routes[] + {\
			\"route\": \"/admin\"},\
			.routes[] + {\
			\"route\": \"/terminator\"}\
		]\
		}" < "./template.json" > "./result_admin.json"
}

generate_spec_json() {
	printf "Generating 'spec.json'\n"

	for var in "${VARYING[@]}"; do
		for t_idx in "${!TENANT_IDS[@]}"; do
			local jq_str
			local tenant=$(printf "%s-%03d" "${TENANT_IDS[$t_idx]}" "$var")
			local port=${ports[$tenant]}

			jq_str=". + {
				\"name\": \"$tenant\",\
				\"port\": $port"
			
			if [ -n "$MTDS_REPL_PERIODS_us" ]; then 
				repl_period=${repl_periods[$tenant]}
				budget=${max_budgets[$tenant]}
				jq_str+=",\"replenishment-period-us\": $repl_period,\"max-budget-us\": $budget"
			fi
			if [ -n "$MTDBF_RESERVATIONS_p" ]; then 
				reservation=${reservations[$tenant]}
				jq_str+=",\"reservation-percentile\": $reservation" 
			fi

			jq_str+=",\"routes\": ["

			local t_routes
			IFS=' ' read -r -a t_routes <<< ${ROUTES[$t_idx]}

			for index in "${!t_routes[@]}"; do
				local route=${t_routes[$index]}
				local workload="$tenant-$route"
				local wasm_path=${wasm_paths[$workload]}
				local resp_content_type=${resp_content_types[$workload]}
				local deadline=${deadlines[$workload]}

				jq_str+=".routes[] + {\
					\"route\": \"/$route\",\
					\"path\": \"$wasm_path\",\
					\"admissions-percentile\": $ESTIMATIONS_PERCENTILE,\
					\"relative-deadline-us\": $deadline,\
					\"http-resp-content-type\": \"$resp_content_type\""
				
				if [ -n "$PREPROCESS_WASM_PATHS" ]; then 
					local preprocess_wasm_path=${preprocess_wasm_paths[$workload]}
					local model_bias=${model_biases[$workload]}
					local model_scale=${model_scales[$workload]}
					local model_num_of_param=${model_num_of_params[$workload]}
					local model_beta1=${model_beta1s[$workload]}
					local model_beta2=${model_beta2s[$workload]}

					jq_str+=",
						\"path_preprocess\": \"$preprocess_wasm_path\",\
						\"model-bias\": $model_bias,\
						\"model-scale\": $model_scale,\
						\"model-num-of-param\": $model_num_of_param,\
						\"model-beta1\": $model_beta1,\
						\"model-beta2\": $model_beta2"
				fi

				jq_str+="}"

				if [ "$index" != $((${#t_routes[@]}-1)) ]; then
					jq_str+=","
				fi
			done
			jq_str+="]}"

			jq "$jq_str" < "./template.json" > "./result_${tenant}.json"
		done
	done

	if [ "$ADMIN_ACCESS" == true ]; then jq_admin_spec; fi

	# Merges all of the multiple specs for a single module
	jq -s '. | sort_by(.name)' ./result_*.json > "./spec.json"
	rm ./result_*.json
}
