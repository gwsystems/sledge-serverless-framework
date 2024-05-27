#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success rate
# Success - The percentage of requests that complete by their deadlines
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (us) of successful requests at the p50, p90, p99, and p100 percentiles

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../bash_libraries"
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

declare -gi iterations=10000
declare -gi duration_sec=5
declare -ga concurrency=(1 18 20 24 28 32 36 40 50)

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


	printf "Running Experiments:\n"
	for con in "${concurrency[@]}"; do
		printf "\t%d Concurrency: " "$con"
		hey -disable-compression -disable-keepalive -disable-redirects -z "$duration_sec"s -n "$iterations" -c "$con" -o csv -m POST -d "30\n" "http://$hostname:10030/fib" > "$results_directory/con$con.csv" 2> /dev/null || {
			printf "[ERR]\n"
			panic "experiment failed"
			return 1
		}
		get_result_count "$results_directory/con$con.csv" || {
			printf "[ERR]\n"
			panic "con$con.csv unexpectedly has zero requests"
			return 1
		}
		printf "[OK]\n"
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
	printf "Concurrency,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Concurrency,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv" "Con"

	for con in "${concurrency[@]}"; do

		if [[ ! -f "$results_directory/con$con.csv" ]]; then
			printf "[ERR]\n"
			error_msg "Missing $results_directory/con$con.csv"
			return 1
		fi

		# Calculate Success Rate for csv (percent of requests that return 200)
		# P.S. When using hey -z option, this result is meaningless
		awk -F, '
		$7 == 200 {ok++}
		END{printf "'"$con"',%3.2f\n", (ok / (NR - 1) * 100)}
	' < "$results_directory/con$con.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to us, and sort
		awk -F, '$7 == 200 {print ($1 * 1000000)}' < "$results_directory/con$con.csv" \
			| sort -g > "$results_directory/con$con-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/con$con-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic duration_sec value?
		duration=$(tail -n1 "$results_directory/con$con.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%d,%d\n" "$con" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/con$con-response.csv" "$results_directory/latency.csv" "$con"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/con$con-response.csv"
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

experiment_server_post() {
	local -r results_directory="$1"

	# Only process data if SLEDGE_SANDBOX_PERF_LOG was set when running sledgert
	if [[ -n "$SLEDGE_SANDBOX_PERF_LOG" ]]; then
		if [[ -f "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" ]]; then
			mv "$__run_sh__base_path/$SLEDGE_SANDBOX_PERF_LOG" "$results_directory/perf.log"
			# process_server_results "$results_directory" || return 1
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

framework_init "$@"
