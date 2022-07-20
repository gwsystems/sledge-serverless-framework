# shellcheck shell=bash
# shellcheck disable=SC2154
if [ -n "$__generate_spec_json_sh__" ]; then return; fi
__generate_spec_json_sh__=$(date)


generate_spec_json() {
	printf "Generating 'spec.json'\n"

	for tenant in "${TENANTS[@]}"; do
		port=${PORTS[$tenant]}
		repl_period=${MTDS_REPLENISH_PERIODS_us[$tenant]}
		budget=${MTDS_MAX_BUDGETS_us[$tenant]}
		# reservation=${MTDBF_RESERVATIONS_percen[${tenant}]}
		route=${ROUTES[$tenant]}
		workload="$tenant-$route"
		deadline=${DEADLINES_us[$workload]}
		expected=${EXPECTED_EXECUTIONS_us[$workload]}

		# Generates unique module specs on different ports using the given 'ru's
		jq ". + { \
			\"name\": \"$tenant\",\
			\"port\": $port,\
			\"replenishment-period-us\": $repl_period, \
			\"max-budget-us\": $budget} | \
			(.routes[] = \
				.routes[] + { \
				\"route\": \"/$route\",\
				\"admissions-percentile\": $ESTIMATIONS_PERCENTILE,\
				\"expected-execution-us\": $expected,\
				\"relative-deadline-us\": $deadline 
				}) \
			" \
			< "./template.json" \
			> "./result_${tenant}.json"
		# \"reservation-percentile\": $reservation, \
	done

	if [ "$CLIENT_TERMINATE_SERVER" == true ]; then
		jq ". + { \
			\"name\": \"Admin\",\
			\"port\": 55555,\
			\"replenishment-period-us\": 0, \
			\"max-budget-us\": 0} | \
			(.routes = [\
				.routes[] + { \
				\"route\": \"/main\",\
				\"admissions-percentile\": 70,\
				\"expected-execution-us\": 1000,\
				\"relative-deadline-us\": 10000}, \
				.routes[] + { \
				\"route\": \"/terminator\",\
				\"admissions-percentile\": 70,\
				\"expected-execution-us\": 1000,\
				\"relative-deadline-us\": 10000 }\
				]) \
			" \
			< "./template.json" \
			> "./result_admin.json"
	fi

	# Merges all of the multiple specs for a single module
	jq -s '. | sort_by(.name)' ./result_*.json > "./spec.json"
	rm ./result_*.json
}
