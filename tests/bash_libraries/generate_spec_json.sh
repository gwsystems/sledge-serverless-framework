# shellcheck shell=bash
# shellcheck disable=SC2154,SC2155
if [ -n "$__generate_spec_json_sh__" ]; then return; fi
__generate_spec_json_sh__=$(date)

jq_admin_spec() {
	jq ". + {\
		\"name\": \"Admin\",\
		\"port\": 55555,\
		\"replenishment-period-us\": 0,\
		\"max-budget-us\": 0,\
		\"reservation-percentile\": 0,\
		\"routes\": [\
			.routes[] + {\
			\"route\": \"/admin\",\
			\"admissions-percentile\": 50,\
			\"expected-execution-us\": 1000,\
			\"relative-deadline-us\": 10000},\
			.routes[] + {\
			\"route\": \"/terminator\",\
			\"admissions-percentile\": 50,\
			\"expected-execution-us\": 1000,\
			\"relative-deadline-us\": 10000}\
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
			local repl_period=${repl_periods[$tenant]}
			local budget=${max_budgets[$tenant]}
			local reservation=${reservations[$tenant]}

			jq_str=". + {
				\"name\": \"$tenant\",\
				\"port\": $port,\
				\"replenishment-period-us\": $repl_period,\
				\"max-budget-us\": $budget,\
				\"reservation-percentile\": $reservation,\
				\"routes\": ["

			local t_routes
			IFS=' ' read -r -a t_routes <<< ${ROUTES[$t_idx]}

			for index in "${!t_routes[@]}"; do
				local route=${t_routes[$index]}
				local workload="$tenant-$route"
				local wasm_path=${wasm_paths[$workload]}
				local resp_content_type=${resp_content_types[$workload]}
				local expected=${expected_execs[$workload]}
				local deadline=${deadlines[$workload]}

				jq_str+=".routes[] + {\
					\"route\": \"/$route\",\
					\"path\": \"$wasm_path\",\
					\"admissions-percentile\": $ESTIMATIONS_PERCENTILE,\
					\"expected-execution-us\": $expected,\
					\"relative-deadline-us\": $deadline,\
					\"http-resp-content-type\": \"$resp_content_type\"}"
				
				if [ "$index" != $((${#t_routes[@]}-1)) ]; then
					jq_str+=","
				fi
			done
			jq_str+="]}"

			jq "$jq_str" < "./template.json" > "./result_${tenant}.json"
		done
	done

	jq_admin_spec

	# Merges all of the multiple specs for a single module
	jq -s '. | sort_by(.name)' ./result_*.json > "./spec.json"
	rm ./result_*.json
}
