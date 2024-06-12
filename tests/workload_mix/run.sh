#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Success - The percentage of requests that complete by their deadlines
# 	TODO: Does this handle non-200s?
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (unit?) of successful requests at the p50, p90, p99, and p100 percetiles

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
# source generate_gnuplots.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source percentiles_table.sh || exit 1

validate_dependencies hey

# Sends requests until the per-module perf window buffers are full
# This ensures that Sledge has accurate estimates of execution time
run_samples() {
	if (($# != 1)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	fi

	local hostname="${1}"

	# Scrape the perf window size from the source if possible
	# TODO: Make a util function
	local -r perf_window_path="$(path_join "$__run_sh__base_path" ../../runtime/include/perf_window_t.h)"
	local -i PERF_WINDOW_CAPACITY
	if ! PERF_WINDOW_CAPACITY=$(grep "PERF_WINDOW_CAPACITY =" < "$perf_window_path" | cut -d\  -f3); then
		printf "Failed to scrape PERF_WINDOW_CAPACITY from ../../runtime/include/perf_window.h\n"
		printf "Defaulting to 16\n"
		PERF_WINDOW_CAPACITY=16
	fi
	local -ir PERF_WINDOW_CAPACITY

	printf "Running Samples: "
	hey -disable-compression -disable-keepalive -disable-redirects -n "$PERF_WINDOW_CAPACITY" -c "$PERF_WINDOW_CAPACITY" -cpus 3 -t 0 -o csv -m POST -d "40\n" "http://${hostname}:10000/fibonacci_10" 1> /dev/null 2> /dev/null || {
		printf "[ERR]\n"
		panic "fibonacci_40 samples failed with $?"
		return 1
	}

	hey -disable-compression -disable-keepalive -disable-redirects -n "$PERF_WINDOW_CAPACITY" -c "$PERF_WINDOW_CAPACITY" -cpus 3 -t 0 -o csv -m POST -d "10\n" "http://${hostname}:10000/fibonacci_10" 1> /dev/null 2> /dev/null || {
		printf "[ERR]\n"
		panic "fibonacci_10 samples failed with $?"
		return 1
	}

	printf "[OK]\n"
	return 0
}

# Execute the fib10 and fib40 experiments sequentially and concurrently
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

	local -a workloads=()

	local -Ar path=(
		[fibonacci_10]=/fibonacci_10
		[fibonacci_40]=/fibonacci_40
	)

	local -Ar body=(
		[fibonacci_10]=10
		[fibonacci_40]=40
	)

	local -A floor=()
	local -A length=()
	local -i total=0

	local -a buffer=()
	local workload=""
	local -i odds=0
	while read -r line; do
		# Read into buffer array, splitting on commas
		readarray -t -d, buffer < <(echo -n "$line")
		# Use human friendly names
		odds="${buffer[0]}"
		workload="${buffer[1]}"
		# Update workload mix structures
		workloads+=("$workload")
		floor+=(["$workload"]=$total)
		length+=(["$workload"]=$odds)
		((total += odds))
	done < mix.csv

	declare -ir random_max=32767
	# Validate Workload Mix
	if ((total <= 0 || total > random_max)); then
		echo "total must be between 1 and $random_max"
		exit 1
	fi

	# TODO: Check that workload is in spec.json
	local -ir batch_size=1
	local -i batch_id=0
	local -i roll=0
	local -ir total_iterations=1000
	local -ir worker_max=50
	local pids

	printf "Running Experiments: "

	# Select a random workload using the workload mix and run command, writing output to disk
	for ((i = 0; i < total_iterations; i += batch_size)); do
		# Block waiting for a worker to finish if we are at our max
		while (($(pgrep --count hey) >= worker_max)); do
			wait -n $(pgrep hey | tr '\n' ' ')
		done
		roll=$((RANDOM % total))
		((batch_id++))
		for workload in "${workloads[@]}"; do
			if ((roll >= floor[$workload] && roll < floor[$workload] + length[$workload])); then
				hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m POST -d "${body[$workload]}\n" "http://${hostname}:10000${path[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" 2> /dev/null &
				break
			fi
		done
	done
	pids=$(pgrep hey | tr '\n' ' ')
	[[ -n $pids ]] && wait -f $pids
	printf "[OK]\n"

	for workload in "${workloads[@]}"; do
		tail --quiet -n +2 "$results_directory/${workload}"_*.csv >> "$results_directory/${workload}.csv"
		rm "$results_directory/${workload}"_*.csv
	done

	return 0
}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_results() {
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
	percentiles_table_header "$results_directory/latency.csv"

	local -ar payloads=(fibonacci_10 fibonacci_40)
	for payload in "${payloads[@]}"; do

		# Filter on 200s, subtract DNS time, convert from s to ms, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000)}' < "$results_directory/$payload.csv" \
			| sort -g > "$results_directory/$payload-response.csv"

		oks=$(wc -l < "$results_directory/$payload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$payload-response.csv" "$results_directory/latency.csv" "$payload"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$payload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

# Expected Symbol used by the framework
experiment_client() {
	local -r target_hostname="$1"
	local -r results_directory="$2"

	run_samples "$target_hostname" || return 1
	run_experiments "$target_hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1

	return 0
}

framework_init "$@"
