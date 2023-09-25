#!/bin/bash

# shellcheck disable=SC1091,SC2034,SC2153,SC2154

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success rate
# Success - The percentage of requests that complete by their deadlines
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (us) of successful requests at the p50, p90, p99, and p100 percentiles

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "$0")")"
# __run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
# __run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source generate_gnuplots.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source percentiles_table.sh || exit 1
source experiment_globals.sh || exit 1
source generate_spec_json.sh || exit 1

validate_dependencies hey loadtest gnuplot jq

# Execute the experiments concurrently
run_experiments() {
	assert_run_experiments_args "$@"

	local -r hostname="$1"
	local -r results_directory="$2"
	local -r loadgen="$3"

	printf "Running Experiments with %s\n" "$loadgen"

	for var in "${VARYING[@]}"; do
		for t_idx in "${!TENANT_IDS[@]}"; do
			local tenant_id=${TENANT_IDS[$t_idx]}
			local tenant=$(printf "%s-%03d" "$tenant_id" "$var")
			local port=${ports[$tenant]}

			local t_routes
			IFS=' ' read -r -a t_routes <<< "${ROUTES[$t_idx]}"

			for index in "${!t_routes[@]}"; do
				local route=${t_routes[$index]}
				local workload="$tenant-$route"
				local expected=${expected_execs[$workload]}
				local deadline=${deadlines[$workload]}
				local arg=${args[$workload]}
				local con=${concurrencies[$workload]}
				local rps=${rpss[$workload]}

				echo "CON for $workload" : "$con"
				echo "RPS for $workload" : "$rps"
				
				local pid
				local -a pids # Run concurrently
				local -A pid_workloads # Run concurrently

				if [ "$loadgen" = "hey" ]; then
					local arg_opt_hey=${arg_opts_hey[$workload]}
					hey $HEY_OPTS -z "$DURATION_sec"s -c "$con" -t 0 -o csv -m POST "$arg_opt_hey" "$arg" "http://${hostname}:$port/$route" > "$results_directory/$workload.csv" 2> "$results_directory/$workload-err.dat" &
				elif [ "$loadgen" = "loadtest" ]; then
					local arg_opt_lt=${arg_opts_lt[$workload]}
					loadtest -t "$DURATION_sec" -c "$con" --rps "$rps" "$arg_opt_lt" "$arg" "http://${hostname}:${port}/$route" > "$results_directory/$workload.dat" 2> "$results_directory/$workload-err.dat" &
				fi
				pid="$!"
				pids+=("$pid")
				pid_workloads+=([$pid]=$workload)
			done
		done

		for ((i=${#pids[@]}-1; i>=0; i--)); do
			local pid=${pids[$i]}
			local pid_workload=${pid_workloads[$pid]}
			wait -f "$pid" || {
				printf "\t%s: [ERR]\n" "$pid_workload"
				panic "failed to wait -f $pid"
				return 1
			}
			[ "$loadgen" = "hey" ] && (get_result_count "$results_directory/$pid_workload.csv" || {
				printf "\t%s: [ERR]\n" "$pid_workload"
				panic "$pid_workload has zero requests."
				return 1
			})
			printf "\t%s: [OK]\n" "$pid_workload"
		done

		unset pids pid_workloads
	done

	if [ "$CLIENT_TERMINATE_SERVER" == true ]; then
		printf "Sent a Terminator to the server\n"
		echo "5" | http "$hostname":55555/terminator &> /dev/null
	fi

	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_client_results_hey() {
	assert_process_client_results_args "$@"

	local -r results_directory="$1"

	printf "Processing HEY Results: "

	# Write headers to CSVs
	for t_id in "${TENANT_IDS[@]}"; do
		printf "Workload,Scs%%,TOTAL,ClientScs,All200,AllFail,Deny,MisDL,Shed,MiscErr\n" >> "$results_directory/success_$t_id.csv" 
		printf "Workload,Throughput\n" >> "$results_directory/throughput_$t_id.csv"
		percentiles_table_header "$results_directory/latency_$t_id.csv" "Workload"
	done

	for workload in "${workloads[@]}"; do
		local t_id=${workload_tids[$workload]}
		local deadline=${workload_deadlines[$workload]}
		local var=${workload_vars[$workload]}
		
		# Some requests come back with an "Unsolicited response ..." See issue #185
		misc_err=$(wc -l < "$results_directory/$workload-err.dat")

		if [ ! -s "$results_directory/$workload-err.dat" ]; then
			# The error file is empty. So remove it.
			rm "$results_directory/$workload-err.dat"
		fi

		# Calculate Success Rate for csv (percent of requests that return 200 within deadline)
		awk -v misc_err="$misc_err" -F, '
			$7 == 200 && ($1 * 1000000) <= '"$deadline"' {ok++}
			$7 == 200 {all200++}
			$7 != 200 {total_failed++}
			$7 == 429 {denied++}
			$7 == 408 {missed_dl++}
			$7 == 409 {killed++}
			END{printf "'"$var"',%3.1f,%d,%d,%d,%d,%d,%d,%d,%d\n", (all200*100/(NR-1+misc_err)), (NR-1+misc_err), ok, all200, (total_failed-1+misc_err), denied, missed_dl, killed, misc_err}
		' < "$results_directory/$workload.csv" >> "$results_directory/success_$t_id.csv"

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
		printf "%s,%d\n" "$var" "$throughput" >> "$results_directory/throughput_$t_id.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency_$t_id.csv" "$var"
		# Delete scratch file used for sorting/counting
		rm "$results_directory/$workload-response.csv" "$results_directory/$workload-response-200.csv"
	done

	for t_id in "${TENANT_IDS[@]}"; do
		# Transform csvs to dat files for gnuplot
		csv_to_dat "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
		rm "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
	done

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

	printf "[OK]\n"
	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_client_results_loadtest() {
	assert_process_client_results_args "$@"

	local -r results_directory="$1"

	printf "Processing Loadtest Results: "

	# Write headers to CSVs
	for t_id in "${TENANT_IDS[@]}"; do
		printf "Workload,Scs%%,TOTAL,All200,AllFail,Deny,MisDL,Shed,MiscErr\n" >> "$results_directory/success_$t_id.csv" 
		printf "Workload,Throughput\n" >> "$results_directory/throughput_$t_id.csv"
		percentiles_table_header "$results_directory/latency_$t_id.csv" "Workload"
	done

	for workload in "${workloads[@]}"; do
		local t_id=${workload_tids[$workload]}
		local var=${workload_vars[$workload]}

		if [ ! -s "$results_directory/$workload-err.dat" ]; then
			# The error file is empty. So remove it.
			rm "$results_directory/$workload-err.dat"
		fi

		if [[ ! -f "$results_directory/$workload.dat" ]]; then
			printf "[ERR]\n"
			error_msg "Missing $results_directory/$workload.dat"
			return 1
		fi

		# Get Number of 200s and then calculate Success Rate (percent of requests that return 200)
		total=$(grep "Completed requests:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		total_failed=$(grep "Total errors:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		denied=$(grep "429:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		missed_dl=$(grep "408:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		killed=$(grep "409:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		misc_err=$(grep "\-1:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)
		all200=$((total - total_failed))

		# ((all200 == 0)) && continue # If all errors, skip line
		success_rate=$(echo "scale=2; $all200*100/$total" | bc)
		printf "%s,%3.1f,%d,%d,%d,%d,%d,%d,%d\n" "$var" "$success_rate" "$total" "$all200" "$total_failed" "$denied" "$missed_dl" "$killed" "$misc_err" >> "$results_directory/success_$t_id.csv"

		# Throughput is calculated as the mean number of successful requests per second
		duration=$(grep "Total time:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)
		printf -v duration %.0f "$duration"
		# throughput=$(grep "Requests per second" "$results_directory/${workload}.dat" | tr -s ' '  | cut -d ' ' -f 14 | tail -n 1) # throughput of ALL
		throughput=$(echo "$all200/$duration" | bc)
		printf "%s,%d\n" "$var" "$throughput" >> "$results_directory/throughput_$t_id.csv"

		# Generate Latency Data
		min=$(echo "$(grep "Minimum latency:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)*1000" | bc)
		p50=$(echo "$(grep 50% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" | bc)
		p90=$(echo "$(grep 90% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" | bc)
		p99=$(echo "$(grep 99% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12)*1000" | bc)
		p100=$(echo "$(grep 100% "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 12 | tail -n 1)*1000" | bc)
		mean=$(echo "scale=1;$(grep "Mean latency:" "$results_directory/${workload}.dat" | tr -s ' ' | cut -d ' ' -f 13)*1000" | bc)

		printf "%s,%d,%d,%.1f,%d,%d,%d,%d\n" "$var" "$total" "$min" "$mean" "$p50" "$p90" "$p99" "$p100" >> "$results_directory/latency_$t_id.csv"
	done

	for t_id in "${TENANT_IDS[@]}"; do
		# Transform csvs to dat files for gnuplot
		csv_to_dat "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
		rm "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
	done

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

	printf "[OK]\n"
	return 0
}

process_server_results() {
	assert_process_server_results_args "$@"

	local -r results_directory="${1:?results_directory not set}"

	printf "Processing Server Results: \n"

	local -r num_of_lines=$(wc -l < "$results_directory/$SERVER_LOG_FILE")
	if [ "$num_of_lines" == 1 ]; then
		printf "No results to process! Exiting the script."
		return 1
	fi

	# Write headers to CSVs
	for t_id in "${TENANT_IDS[@]}"; do
		printf "Workload,Scs%%,TOTAL,SrvScs,All200,AllFail,DenyBE,DenyG,xDenyBE,xDenyG,MisD_Glb,MisD_Loc,MisD_WB,Shed_Glb,Shed_Loc,Shed_WB,Misc\n" >> "$results_directory/success_$t_id.csv" 
		printf "Workload,Throughput\n" >> "$results_directory/throughput_$t_id.csv"
		percentiles_table_header "$results_directory/latency_$t_id.csv" "Workload"

		# Write headers to CSVs
		for metric in "${SANDBOX_METRICS[@]}"; do
			percentiles_table_header "$results_directory/${metric}_$t_id.csv" "Workload"
		done
		percentiles_table_header "$results_directory/running_user_200_$t_id.csv" "Workload"
		# percentiles_table_header "$results_directory/running_user_nonzero_$t_id.csv" "Workload"
		percentiles_table_header "$results_directory/total_200_$t_id.csv" "Workload"
		# percentiles_table_header "$results_directory/memalloc_$t_id.csv" "Workload"
	done

	for workload in "${workloads[@]}"; do
		mkdir -p "$results_directory/$workload"

		local t_id=${workload_tids[$workload]}
		local deadline=${workload_deadlines[$workload]}
		local var=${workload_vars[$workload]}
		
		for metric in "${SANDBOX_METRICS[@]}"; do
			awk -F, '
				{workload = sprintf("%s-%s", $'"$SANDBOX_TENANT_NAME_FIELD"', substr($'"$SANDBOX_ROUTE_FIELD"',2))}
				workload == "'"$workload"'" {printf("%d,%d\n", $'"${SANDBOX_METRICS_FIELDS[$metric]}"' / $'"$SANDBOX_CPU_FREQ_FIELD"', $'"$SANDBOX_RESPONSE_CODE_FIELD"')}
			' < "$results_directory/$SERVER_LOG_FILE" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}_$t_id.csv" "$workload"

			# Delete scratch file used for sorting/counting
			# rm "$results_directory/$workload/${metric}_sorted.csv"
		done

		awk -F, '$2 == 200 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/running_user_sorted.csv" > "$results_directory/$workload/running_user_200_sorted.csv"
		percentiles_table_row "$results_directory/$workload/running_user_200_sorted.csv" "$results_directory/running_user_200_$t_id.csv" "$workload"
		# awk -F, '$1 > 0 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/running_user_sorted.csv" > "$results_directory/$workload/running_user_nonzero_sorted.csv"
		# percentiles_table_row "$results_directory/$workload/running_user_nonzero_sorted.csv" "$results_directory/running_user_nonzero_$t_id.csv" "$workload"
		awk -F, '$2 == 200 {printf("%d,%d\n", $1, $2)}' < "$results_directory/$workload/total_sorted.csv" > "$results_directory/$workload/total_200_sorted.csv"
		percentiles_table_row "$results_directory/$workload/total_200_sorted.csv" "$results_directory/total_200_$t_id.csv" "$workload"

		# Memory Allocation
		# awk -F, '$2 == "'"$workload"'" {printf("%.0f\n", $MEMORY_FIELD)}' < "$results_directory/$SERVER_LOG_FILE" | sort -g > "$results_directory/$workload/memalloc_sorted.csv"

		# percentiles_table_row "$results_directory/$workload/memalloc_sorted.csv" "$results_directory/memalloc_$t_id.csv.csv" "$workload" "%1.0f"

		# Calculate Success Rate for csv (percent of requests that complete), $1 and deadline are both in us, so not converting
		awk -F, '
			$2 == 200 && $1 <= '"$deadline"' {ok++}
			$2 == 200 {all200++}
			$2 != 200 {total_failed++}
			$2 == 4290 {denied_any++}
			$2 == 4291 {denied_gtd++}
			$2 == 4295 {x_denied_any++}
			$2 == 4296 {x_denied_gtd++}
			$2 == 4080 {mis_dl_glob++}
			$2 == 4081 {mis_dl_local++}
			$2 == 4082 {mis_dl_wb++}
			$2 == 4090 {shed_glob++}
			$2 == 4091 {shed_local++}
			$2 == 4092 {shed_wb++}
			$2 == 4093 {misc++}
			END{printf "'"$var"',%3.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", (all200*100/NR), NR, ok, all200, total_failed, denied_any, denied_gtd, x_denied_any, x_denied_gtd, mis_dl_glob, mis_dl_local, mis_dl_wb, shed_glob, shed_local, shed_wb, misc}
		' < "$results_directory/$workload/total_sorted.csv" >> "$results_directory/success_$t_id.csv"

		# Throughput is calculated on the client side, so ignore the below line
		printf "%s,%d\n" "$var" "1" >> "$results_directory/throughput_$t_id.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload/total_sorted.csv" "$results_directory/latency_$t_id.csv" "$var"

		# Delete scratch file used for sorting/counting
		# rm "$results_directory/$workload/memalloc_sorted.csv"

		# Delete directory
		# rm -rf "${results_directory:?}/${workload:?}"
	done

	for t_id in "${TENANT_IDS[@]}"; do
		for metric in "${SANDBOX_METRICS[@]}"; do
			csv_to_dat "$results_directory/${metric}_$t_id.csv"
			rm "$results_directory/${metric}_$t_id.csv"
		done

		csv_to_dat "$results_directory/running_user_200_$t_id.csv" "$results_directory/total_200_$t_id.csv" # "$results_directory/running_user_nonzero_$t_id.csv"
		rm "$results_directory/running_user_200_$t_id.csv" "$results_directory/total_200_$t_id.csv" # "$results_directory/running_user_nonzero_$t_id.csv"

		# Transform csvs to dat files for gnuplot
		# csv_to_dat "$results_directory/memalloc$t_id.csv"
		csv_to_dat "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
		# rm "$results_directory/memalloc$t_id.csv"
		rm "$results_directory/success_$t_id.csv" "$results_directory/throughput_$t_id.csv" "$results_directory/latency_$t_id.csv"
	done

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

	printf "[OK]\n"
	return 0
}

process_server_http_results() {
	assert_process_server_results_args "$@"

	local -r results_directory="${1:?results_directory not set}"

	printf "Processing Server HTTP Results: \n"

	local -r num_of_lines=$(wc -l < "$results_directory/$SERVER_HTTP_LOG_FILE")
	if [ "$num_of_lines" == 1 ]; then
		printf "No results to process! Exiting the script."
		return 1
	fi

	for metric in "${HTTP_METRICS[@]}"; do
		percentiles_table_header "$results_directory/$metric.csv" "workload"
	done

	for workload in "${workloads[@]}"; do
		mkdir -p "$results_directory/$workload"
		local var=${workload_vars[$workload]}

		for metric in "${HTTP_METRICS[@]}"; do
			awk -F, '
				{workload = sprintf("%s-%s", $'"$HTTP_TENANT_NAME_FIELD"', substr($'"$HTTP_ROUTE_FIELD"',2))}
				workload == "'"$workload"'" {printf("%.1f\n", $'"${HTTP_METRICS_FIELDS[$metric]}"' / $'"$HTTP_CPU_FREQ_FIELD"')}
			' < "$results_directory/$SERVER_HTTP_LOG_FILE" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}.csv" "$var"

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
