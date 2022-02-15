#!/bin/bash

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

validate_dependencies hey gnuplot jq

# The global configs for the scripts
declare -r ITERATIONS=10000 # ignored when DURATION_SEC is used
declare -r DURATION_SEC=5
declare -r NWORKERS=18
declare -r APP=fib30
declare -r INIT_PORT=20000
declare -r EXPECTED_EXEC_US=6500
declare -r DEADLINE_US=10000
declare -r REPL_PERIOD_US=10000
declare -r RESERVATION_UTILS=(5 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100)
# declare -ar RESERVATION_UTILS=(5 100) # for quick testing

generate_spec() {
	printf "Generating 'spec.json'...(may take a couple sec)\n"

	local -i max_budget_us
	local -i port

	for ru in "${RESERVATION_UTILS[@]}"; do
		workload=$(printf "${APP}_%03dp" ${ru})
		port=$(($INIT_PORT+$ru))
		max_budget_us=$((REPL_PERIOD_US*NWORKERS*ru/100))

		# Generates unique module specs on different ports using the given 'ru's
		jq ". + { \
		\"name\": \"${workload}\",\
		\"port\": ${port},\
		\"expected-execution-us\": ${EXPECTED_EXEC_US},\
		\"relative-deadline-us\": ${DEADLINE_US},\
		\"replenishment-period-us\": ${REPL_PERIOD_US}, \
		\"max-budget-us\": ${max_budget_us}}" \
			< "./template.json" \
			> "./result_${ru}.json"
	done

	jq ". + { \
		\"name\": \"fib30\",\
		\"port\": 10030,\
		\"expected-execution-us\": ${EXPECTED_EXEC_US},\
		\"relative-deadline-us\": ${DEADLINE_US},\
		\"replenishment-period-us\": 0, \
		\"max-budget-us\": 0}" \
			< "./template.json" \
			> "./result_000.json"

	# Merges all of the multiple specs for a single module
	jq -s '. | sort_by(.name)' ./result_*.json > "./spec.json"
	rm ./result_*.json
}


# Execute the experiments concurrently
# $1 (hostname)
# $2 (results_directory) - a directory where we will store our results
run_experiments() {
	if (($# != 2)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	elif [[ ! -d "$2" ]]; then
		panic "directory \"$2\" does not exist"
		return 1
	fi

	local hostname="$1"
	local results_directory="$2"

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir OFFSET=2

	printf "Running Experiments\n"

	# Run concurrently
	# The lower priority has OFFSETs to ensure it runs the entire time the high priority is trying to run
	# This asynchronously trigger jobs and then wait on their pids
	local fib30_nk_PID
	local fib30_PID

	for ru in "${RESERVATION_UTILS[@]}"; do
		workload=$(printf "${APP}_%03dp" ${ru})
		port=$(($INIT_PORT+$ru))

		hey -disable-compression -disable-keepalive -disable-redirects -z "$(($DURATION_SEC+$OFFSET+1))"s -n "$ITERATIONS" -c 90 -t 0 -o csv -m GET -d "30\n" "http://${hostname}:10030" > "$results_directory/fib30.csv" 2> /dev/null &
		fib30_PID="$!"

		sleep "$OFFSET"s
		
		hey -disable-compression -disable-keepalive -disable-redirects -z "$DURATION_SEC"s -n "$ITERATIONS" -c 18 -t 0 -o csv -m GET -d "30\n" "http://${hostname}:${port}" > "$results_directory/$workload.csv" 2> /dev/null &
		fib30_nk_PID="$!"

		wait -f "$fib30_nk_PID" || {
			printf "\t$workload: [ERR]\n"
			panic "failed to wait -f ${fib30_nk_PID}"
			return 1
		}
		get_result_count "$results_directory/$workload.csv" || {
			printf "\t$workload: [ERR]\n"
			panic "$workload has zero requests."
			return 1
		}
		printf "\t$workload: [OK]\n"

		wait -f "$fib30_PID" || {
			printf "\tfib30: [ERR]\n"
			panic "failed to wait -f ${fib30_PID}"
			return 1
		}
		get_result_count "$results_directory/fib30.csv" || {
			printf "\tfib30: [ERR]\n"
			panic "fib30 has zero requests."
			return 1
		}
		# printf "\tfib30: [OK]\n"
	done

	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_client_results() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	local -r results_directory="$1"

	printf "Processing Results: "

	# Write headers to CSVs
	printf "Workload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv"

	for ru in "${RESERVATION_UTILS[@]}"; do
		workload=$(printf "${APP}_%03dp" ${ru})

		# Calculate Success Rate for csv (percent of requests that return 200 within DEADLINE_US)
		awk -F, '
			$7 == 200 && ($1 * 1000000) <= '"$DEADLINE_US"' {ok++}
			END{printf "'"$ru"',%3.1f\n", (ok / (NR - 1) * 100)}
		' < "$results_directory/$workload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to us, and sort
		awk -F, '$7 == 200 {print ($1 * 1000000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/$workload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic DURATION_SEC value?
		duration=$(tail -n1 "$results_directory/$workload.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%s,%d\n" "$ru" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency.csv" "$ru"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$workload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"
	rm "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

	printf "[OK]\n"
	return 0
}

process_server_results() {
	local -r results_directory="${1:?results_directory not set}"

	if ! [[ -d "$results_directory" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	printf "Processing Server Results: "

	# Write headers to CSVs
	printf "Workload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv"

	local -a metrics=(total queued uninitialized allocated initialized runnable preempted running_sys running_user asleep returned complete error)

	local -A fields=(
		[total]=6
		[queued]=7
		[uninitialized]=8
		[allocated]=9
		[initialized]=10
		[runnable]=11
		[preempted]=12
		[running_sys]=13
		[running_user]=14
		[asleep]=15
		[returned]=16
		[complete]=17
		[error]=18
	)

	# Write headers to CSVs
	for metric in "${metrics[@]}"; do
		percentiles_table_header "$results_directory/$metric.csv" "module"
	done
	percentiles_table_header "$results_directory/memalloc.csv" "module"

	for ru in "${RESERVATION_UTILS[@]}"; do
		workload=$(printf "${APP}_%03dp" ${ru})
		mkdir "$results_directory/$workload"

		# TODO: Only include Complete
		for metric in "${metrics[@]}"; do
			awk -F, '$2 == "'"$workload"'" {printf("%.4f\n", $'"${fields[$metric]}"' / $19)}' < "$results_directory/perf.log" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}.csv" "$workload"

			# Delete scratch file used for sorting/counting
			# rm "$results_directory/$workload/${metric}_sorted.csv"
		done

		# Memory Allocation
		awk -F, '$2 == "'"$workload"'" {printf("%.0f\n", $20)}' < "$results_directory/perf.log" | sort -g > "$results_directory/$workload/memalloc_sorted.csv"

		percentiles_table_row "$results_directory/$workload/memalloc_sorted.csv" "$results_directory/memalloc.csv" "$workload" "%1.0f"


		# Calculate Success Rate for csv (percent of requests that complete), $1 and DEADLINE_US are both in us, so not converting 
		awk -F, '
			$1 <= '"$DEADLINE_US"' {ok++}
			END{printf "'"$ru"',%3.1f\n", (ok / NR * 100)}
		' < "$results_directory/$workload/total_sorted.csv" >> "$results_directory/success.csv"

		# Throughput is calculated on the client side, so ignore the below line
		printf "%s,%d\n" "$ru" "1" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload/total_sorted.csv" "$results_directory/latency.csv" "$ru"
		

		# Delete scratch file used for sorting/counting
		# rm "$results_directory/$workload/memalloc_sorted.csv"

		# Delete directory
		# rm -rf "${results_directory:?}/${workload:?}"

	done

	# Transform csvs to dat files for gnuplot
	for metric in "${metrics[@]}"; do
		csv_to_dat "$results_directory/$metric.csv"
		rm "$results_directory/$metric.csv"
	done
	csv_to_dat "$results_directory/memalloc.csv"
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	rm "$results_directory/memalloc.csv" "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

	printf "[OK]\n"
	return 0
}


experiment_server_post() {
	local -r results_directory="$1"

	# Only process data if SLEDGE_SANDBOX_PERF_LOG was set when running sledgert
	if [[ -n "$SLEDGE_SANDBOX_PERF_LOG" ]]; then
		if [[ -f "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" ]]; then
			mv "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" "$results_directory/perf.log"
			process_server_results "$results_directory" || return 1
		else
			echo "Perf Log was set, but perf.log not found!"
		fi
	fi
}

# Expected Symbol used by the framework
experiment_client() {
	local -r target_hostname="$1"
	local -r results_directory="$2"

	#run_samples "$target_hostname" || return 1
	run_experiments "$target_hostname" "$results_directory" || return 1
	process_client_results "$results_directory" || return 1

	return 0
}

generate_spec
framework_init "$@"
