#!/bin/bash

# shellcheck disable=SC1091,SC2034

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success rate
# Success - The percentage of requests that complete by their deadlines
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (us) of successful requests at the p50, p90, p99, and p100 percentiles

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source generate_gnuplots.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source percentiles_table.sh || exit 1
source experiment_globals.sh || exit 1

validate_dependencies hey loadtest jq

# The global configs for the scripts
declare -r CLIENT_TERMINATE_SERVER=false
declare -r ITERATIONS=10000
declare -r DURATION_sec=1
declare -r ESTIMATIONS_PERCENTILE=70

declare -r TENANT_NG="NNN"
declare -r TENANT_GR="GWU"
declare -ar TENANTS=("$TENANT_NG" "$TENANT_GR")

declare -Ar ARGS=(
	[$TENANT_NG]="30"
	[$TENANT_GR]="30"
)

# Make sure not to use 55555 (Reserved for Admin)
declare -Ar PORTS=(
	[$TENANT_NG]=10030
	[$TENANT_GR]=20030
)

# No need for slashes
declare -Ar ROUTES=(
	[$TENANT_NG]="fib"
	[$TENANT_GR]="fib"
)

declare -Ar MTDS_REPLENISH_PERIODS_us=(
	[$TENANT_NG]=0
	[$TENANT_GR]=16000
)

declare -Ar MTDS_MAX_BUDGETS_us=(
	[$TENANT_NG]=0
	[$TENANT_GR]=144000
)

declare -Ar MTDBF_RESERVATIONS_percen=(
	[$TENANT_NG]=0
	[$TENANT_GR]=0
)

declare -ar WORKLOADS=("${TENANT_NG}-${ROUTES[$TENANT_NG]}" "${TENANT_GR}-${ROUTES[$TENANT_GR]}")

declare -Ar EXPECTED_EXECUTIONS_us=(
	[${WORKLOADS[0]}]=4000
	[${WORKLOADS[1]}]=4000
)

declare -Ar DEADLINES_us=(
	[${WORKLOADS[0]}]=16000
	[${WORKLOADS[1]}]=16000
)


# Generate the spec.json file from the given arguments above
. "$__run_sh__bash_libraries_absolute_path/generate_spec_json.sh"

# Execute the experiments concurrently
run_experiments() {
	assert_run_experiments_args "$@"

	local -r hostname="$1"
	local -r results_directory="$2"
	local -r loadgen="$3"

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir OFFSET=1

	printf "Running Experiments with %s\n" "$loadgen"

	# Run concurrently
	local app_gr_PID
	local app_ng_PID

	local -r port_ng=${PORTS[$TENANT_NG]}
	local -r port_gr=${PORTS[$TENANT_GR]}
	
	local -r route_ng=${ROUTES[$TENANT_NG]}
	local -r route_gr=${ROUTES[$TENANT_GR]}

	local -r workload_ng=${WORKLOADS[0]}
	local -r workload_gr=${WORKLOADS[1]}

	local -r deadline_ng=${DEADLINES_us[$workload_ng]}
	local -r deadline_gr=${DEADLINES_us[$workload_gr]}

	local -r con_ng=$((NWORKERS*10))
	local -r con_gr=$((NWORKERS*2))

	local -r rps_ng=$((1000000*con_ng/deadline_ng))
	local -r rps_gr=$((1000000*con_gr/deadline_gr))

	local -r arg_ng=${ARGS[$TENANT_NG]}
	local -r arg_gr=${ARGS[$TENANT_GR]}

	if [ "$loadgen" = "hey" ]; then
		hey -disable-compression -disable-keepalive -disable-redirects -z $((DURATION_sec+OFFSET))s -n "$ITERATIONS" -c $con_ng -t 0 -o csv -m POST -d "$arg_ng" "http://${hostname}:$port_ng/$route_ng" > "$results_directory/$workload_ng.csv" 2> "$results_directory/$workload_ng-err.dat" &
		app_ng_PID="$!"

		sleep "$OFFSET"s

		hey -disable-compression -disable-keepalive -disable-redirects -z "$DURATION_sec"s -n "$ITERATIONS" -c $con_gr -t 0 -o csv -m POST -d "$arg_gr" "http://${hostname}:$port_gr/$route_gr" > "$results_directory/$workload_gr.csv" 2> "$results_directory/$workload_gr-err.dat" &
		app_gr_PID="$!"
	elif [ "$loadgen" = "loadtest" ]; then
		loadtest -t $((DURATION_sec+OFFSET)) -c $con_ng --rps $rps_ng -P "$arg_ng" "http://${hostname}:$port_ng/$route_ng" > "$results_directory/$workload_ng.dat" 2> "$results_directory/$workload_ng-err.dat" &
		app_ng_PID="$!"

		sleep "$OFFSET"s

		loadtest -t "$DURATION_sec" -c $con_gr --rps $rps_gr -P "$arg_gr" "http://${hostname}:$port_gr/$route_gr" > "$results_directory/$workload_gr.dat" 2> "$results_directory/$workload_gr-err.dat" &
		app_gr_PID="$!"
	fi

	wait -f "$app_gr_PID" || {
		printf "\t%s: [ERR]\n" "$workload_gr"
		panic "failed to wait -f ${app_gr_PID}"
		return 1
	}
	[ "$loadgen" = "hey" ] && (get_result_count "$results_directory/$workload_gr.csv" || {
		printf "\t%s: [ERR]\n" "$workload_gr"
		panic "$workload_gr has zero requests."
		return 1
	})
	printf "\t%s: [OK]\n" "$workload_gr"

	wait -f "$app_ng_PID" || {
		printf "\t%s: [ERR]\n" "$workload_ng"
		panic "failed to wait -f ${app_ng_PID}"
		return 1
	}
	[ "$loadgen" = "hey" ] && (get_result_count "$results_directory/$workload_ng.csv" || {
		printf "\t%s: [ERR]\n" "$workload_ng"
		panic "$workload_ng has zero requests."
		return 1
	})
	printf "\t%s: [OK]\n" "$workload_ng"

	if [ "$CLIENT_TERMINATE_SERVER" == true ]; then
		printf "Sent a Terminator to the server\n"
		echo "55" | http "$hostname":55555/terminator &> /dev/null
	fi

	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_client_results_hey() {
	assert_process_client_results_args "$@"

	local -r results_directory="$1"

	printf "Processing HEY Results: "

	# Write headers to CSVs
	printf "Workload,Scs%%,TOTAL,ClientScs,All200,AllFail,Deny,MisDL,Shed,MiscErr\n" >> "$results_directory/success.csv"
	printf "Workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv" "Workload"
	# percentiles_table_header "$results_directory/latency-200.csv" "Workload"

	for workload in "${WORKLOADS[@]}"; do

		local -i deadline=${DEADLINES_us[$workload]}

		# Some requests come back with an "Unsolicited response ..." See issue #185
		misc_err=$(wc -l < "$results_directory/$workload-err.dat")

		# Calculate Success Rate for csv (percent of requests that return 200 within deadline)
		awk -v misc_err="$misc_err" -F, '
			$7 == 200 && ($1 * 1000000) <= '"$deadline"' {ok++}
			$7 == 200 {all200++}
			$7 != 200 {total_failed++}
			$7 == 429 {denied++}
			$7 == 408 {missed_dl++}
			$7 == 409 {killed++}
			END{printf "'"$workload"',%3.1f,%d,%d,%d,%d,%d,%d,%d,%d\n", (ok/(NR-1+misc_err)*100), (NR-1+misc_err), ok, all200, (total_failed-1+misc_err), denied, missed_dl, killed, misc_err}
		' < "$results_directory/$workload.csv" >> "$results_directory/success.csv"

		# Convert from s to us, and sort
		awk -F, 'NR > 1 {print ($1 * 1000000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		# Filter on 200s, convert from s to us, and sort
		awk -F, '$7 == 200 {print ($1 * 1000000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response-200.csv"

		# Get Number of all entries
		all=$(wc -l < "$results_directory/$workload-response.csv")
		((all == 0)) && continue # If all errors, skip line

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/$workload-response-200.csv")
		# ((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic DURATION_sec value?
		duration=$(tail -n1 "$results_directory/$workload.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%s,%d\n" "$workload" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency.csv" "$workload"
		# percentiles_table_row "$results_directory/$workload-response-200.csv" "$results_directory/latency-200.csv" "$workload"
		# Delete scratch file used for sorting/counting
		rm "$results_directory/$workload-response.csv" "$results_directory/$workload-response-200.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"
	rm "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_client_results_loadtest() {
	assert_process_client_results_args "$@"

	local -r results_directory="$1"

	printf "Processing Loadtest Results: "

	# Write headers to CSVs
	printf "Workload,Scs%%,TOTAL,All200,AllFail,Deny,MisDL,Shed,MiscErr\n" >> "$results_directory/success.csv"
	printf "Workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv" "Workload"

	for workload in "${WORKLOADS[@]}"; do

		if [[ ! -f "$results_directory/$workload.dat" ]]; then
			printf "[ERR]\n"
			error_msg "Missing $results_directory/$workload.dat"
			return 1
		fi

		# Get Number of 200s and then calculate Success Rate (percent of requests that return 200)
		#  If using loadtest -n option (not -t), then use all200/iterations instead of all200/total.
		total=$(grep "Completed requests:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		total_failed=$(grep "Total errors:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		denied=$(grep "429:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		missed_dl=$(grep "408:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		killed=$(grep "409:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		misc_err=$(grep "\-1:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		all200=$((total-total_failed))

		# ((all200 == 0)) && continue # If all errors, skip line
		success_rate=$(echo "scale=2; $all200/$total*100"|bc)
		printf "%s,%3.1f,%d,%d,%d,%d,%d,%d,%d\n" "$workload" "$success_rate" "$total" "$all200" "$total_failed" "$denied" "$missed_dl" "$killed" "$misc_err" >> "$results_directory/success.csv"

		# Throughput is calculated as the mean number of successful requests per second
		duration=$(grep "Total time:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		printf -v duration %.0f "$duration"
		# throughput=$(grep "Requests per second" "$results_directory/${workload}.dat" | tr -s ' '  | cut -d ' ' -f 14 | tail -n 1) # throughput of ALL
		throughput=$(echo "$all200/$duration" | bc)
		printf "%s,%d\n" "$workload" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data
		min=0 # not provided by loadtest
		p50=$(echo "$(grep 50% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" |bc)
		p90=$(echo "$(grep 90% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" |bc)
		p99=$(echo "$(grep 99% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" |bc)
		p100=$(echo "$(grep 100% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12 | tail -n 1)*1000" |bc)
		mean=$(echo "scale=1;$(grep "Mean latency:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)*1000" |bc)

		printf "%s,%d,%d,%.1f,%d,%d,%d,%d\n" "$workload" "$total" "$min" "$mean" "$p50" "$p90" "$p99" "$p100" >> "$results_directory/latency.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"
	rm "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

process_server_results() {
	assert_process_server_results_args "$@"

	local -r results_directory="${1:?results_directory not set}"

	printf "Processing Server Results: \n"

	num_of_lines=$(wc -l < "$results_directory/$SERVER_LOG_FILE")
	if [ "$num_of_lines" == 1 ]; then
		printf "No results to process! Exiting the script."
		return 1
	fi

	# Write headers to CSVs
	printf "Workload,Scs%%,TOTAL,SrvScs,All200,AllFail,DenyAny,DenyG,MisDL_Glb,MisDL_Loc,Shed_Glb,Shed_Loc,QueFull\n" >> "$results_directory/success.csv"
	printf "Workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv" "Workload"

	# Write headers to CSVs
	for metric in "${SANDBOX_METRICS[@]}"; do
		percentiles_table_header "$results_directory/$metric.csv" "Workload"
	done
	percentiles_table_header "$results_directory/running_user_200.csv" "Workload"
	percentiles_table_header "$results_directory/running_user_nonzero.csv" "Workload"
	percentiles_table_header "$results_directory/total_200.csv" "Workload"
	# percentiles_table_header "$results_directory/memalloc.csv" "module"

	for workload in "${WORKLOADS[@]}"; do
		mkdir "$results_directory/$workload"

		local -i deadline=${DEADLINES_us[$workload]}

		for metric in "${SANDBOX_METRICS[@]}"; do
			awk -F, '
				{workload = sprintf("%s-%s", $'"$SANDBOX_TENANT_NAME_FIELD"', substr($'"$SANDBOX_ROUTE_FIELD"',2))}
				workload == "'"$workload"'" {printf("%d,%d\n", $'"${SANDBOX_METRICS_FIELDS[$metric]}"' / $'"$SANDBOX_CPU_FREQ_FIELD"', $'"$SANDBOX_RESPONSE_CODE_FIELD"')}
			' < "$results_directory/$SERVER_LOG_FILE" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}.csv" "$workload"

			# Delete scratch file used for sorting/counting
			# rm "$results_directory/$workload/${metric}_sorted.csv"
		done

		awk -F, '$2 == 200 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/running_user_sorted.csv" > "$results_directory/$workload/running_user_200_sorted.csv"
		percentiles_table_row "$results_directory/$workload/running_user_200_sorted.csv" "$results_directory/running_user_200.csv" "$workload"
		awk -F, '$1 > 0 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/running_user_sorted.csv" > "$results_directory/$workload/running_user_nonzero_sorted.csv"
		percentiles_table_row "$results_directory/$workload/running_user_nonzero_sorted.csv" "$results_directory/running_user_nonzero.csv" "$workload"
		awk -F, '$2 == 200 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/total_sorted.csv" > "$results_directory/$workload/total_200_sorted.csv"
		percentiles_table_row "$results_directory/$workload/total_200_sorted.csv" "$results_directory/total_200.csv" "$workload"


		# Memory Allocation
		# awk -F, '$2 == "'"$workload"'" {printf("%.0f\n", $MEMORY_FIELD)}' < "$results_directory/$SERVER_LOG_FILE" | sort -g > "$results_directory/$workload/memalloc_sorted.csv"

		# percentiles_table_row "$results_directory/$workload/memalloc_sorted.csv" "$results_directory/memalloc.csv" "$workload" "%1.0f"

		# Calculate Success Rate for csv (percent of requests that complete), $1 and deadline are both in us, so not converting
		awk -F, '
			$2 == 200 && $1 <= '"$deadline"' {ok++}
			$2 == 200 {all200++}
			$2 != 200 {total_failed++}
			$2 == 4290 {denied_any++}
			$2 == 4291 {denied_gtd++}
			$2 == 4080 {mis_dl_glob++}
			$2 == 4081 {mis_dl_local++}
			$2 == 4090 {shed_glob++}
			$2 == 4091 {shed_local++}
			$2 == 999 {global_full++}
			END{printf "'"$workload"',%3.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", (ok/NR*100), NR, ok, all200, total_failed, denied_any, denied_gtd, mis_dl_glob, mis_dl_local, shed_glob, shed_local, global_full}
		' < "$results_directory/$workload/total_sorted.csv" >> "$results_directory/success.csv"

		# Throughput is calculated on the client side, so ignore the below line
		printf "%s,%d\n" "$workload" "1" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload/total_sorted.csv" "$results_directory/latency.csv" "$workload"

		# Delete scratch file used for sorting/counting
		# rm "$results_directory/$workload/memalloc_sorted.csv"

		# Delete directory
		# rm -rf "${results_directory:?}/${workload:?}"

	done

	# Transform csvs to dat files for gnuplot
	for metric in "${SANDBOX_METRICS[@]}"; do
		csv_to_dat "$results_directory/$metric.csv"
		rm "$results_directory/$metric.csv"
	done
	csv_to_dat "$results_directory/running_user_200.csv" "$results_directory/running_user_nonzero.csv" "$results_directory/total_200.csv"
	rm "$results_directory/running_user_200.csv" "$results_directory/running_user_nonzero.csv" "$results_directory/total_200.csv"

	# csv_to_dat "$results_directory/memalloc.csv"
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"
	# rm "$results_directory/memalloc.csv" 
	rm "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

process_server_http_results() {
	assert_process_server_results_args "$@"

	local -r results_directory="${1:?results_directory not set}"

	printf "Processing Server HTTP Results: \n"

	num_of_lines=$(wc -l < "$results_directory/$SERVER_HTTP_LOG_FILE")
	if [ "$num_of_lines" == 1 ]; then
		printf "No results to process! Exiting the script."
		return 1
	fi

	for metric in "${HTTP_METRICS[@]}"; do
		percentiles_table_header "$results_directory/$metric.csv" "workload"
	done

	for workload in "${WORKLOADS[@]}"; do
		mkdir -p "$results_directory/$workload"

		for metric in "${HTTP_METRICS[@]}"; do
			awk -F, '
				{workload = sprintf("%s-%s", $'"$HTTP_TENANT_NAME_FIELD"', substr($'"$HTTP_ROUTE_FIELD"',2))}
				workload == "'"$workload"'" {printf("%.1f\n", $'"${HTTP_METRICS_FIELDS[$metric]}"' / $'"$HTTP_CPU_FREQ_FIELD"')}
			' < "$results_directory/$SERVER_HTTP_LOG_FILE" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}.csv" "$workload"

			# Delete directory
			# rm -rf "${results_directory:?}/${workload:?}"
		done
	done

	# Transform CSVs to dat files for gnuplot
	for metric in "${HTTP_METRICS[@]}"; do
		csv_to_dat "$results_directory/$metric.csv"
		rm "$results_directory/$metric.csv"
	done

	printf "[OK]\n"
	return 0
}

experiment_server_post() {
	local -r results_directory="$1"

	# Only process data if SLEDGE_SANDBOX_PERF_LOG was set when running sledgert
	if [[ -n "$SLEDGE_SANDBOX_PERF_LOG" ]]; then
		if [[ -f "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" ]]; then
			mv "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" "$results_directory/$SERVER_LOG_FILE"
			process_server_results "$results_directory" || return 1
			rm "$results_directory/$SLEDGE_SANDBOX_PERF_LOG"
		else
			echo "Sandbox Perf Log was set, but $SERVER_LOG_FILE not found!"
		fi
	fi

	if [[ -n "$SLEDGE_HTTP_SESSION_PERF_LOG" ]]; then
		if [[ -f "$__run_sh__base_path/$SLEDGE_HTTP_SESSION_PERF_LOG" ]]; then
			mv "$__run_sh__base_path/$SLEDGE_HTTP_SESSION_PERF_LOG" "$results_directory/$SERVER_HTTP_LOG_FILE"
			process_server_http_results "$results_directory" || return 1
			rm "$results_directory/$SERVER_HTTP_LOG_FILE"
		else
			echo "HTTP Perf Log was set, but $SERVER_HTTP_LOG_FILE not found!"
		fi
	fi
}

# Expected Symbol used by the framework
experiment_client() {
	local -r target_hostname="$1"
	local -r results_directory="$2"
	local loadgen="$3"

	if [ "$loadgen" = "" ]; then
		loadgen="hey"
		echo "No load generator specified. So, defaulting to HEY!"
	elif [ $loadgen = "lt" ]; then
		loadgen="loadtest"
	fi

	#run_samples "$target_hostname" "$loadgen" || return 1
	run_experiments "$target_hostname" "$results_directory" "$loadgen" || return 1

	if [ "$loadgen" = "hey" ]; then
		process_client_results_hey "$results_directory" || return 1
	elif [ "$loadgen" = "loadtest" ]; then
		process_client_results_loadtest "$results_directory" || return 1
	else
		echo "Unknown load generator \"$loadgen\" was entered!"
	fi

	return 0
}

generate_spec_json
framework_init "$@"
