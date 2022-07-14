#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Success - The percentage of requests that complete out of the total expected
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

validate_dependencies loadtest gnuplot

# The global configs for the scripts
declare -gi iterations=10000
declare -gi duration_sec=5
declare -ga concurrency=(1 9 18 20 30 40 60 80 100)
declare -gi deadline_us=16000 #10ms for fib30

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
		
		rps=$((1000000/$deadline_us*$con))
		
		loadtest -t "$duration_sec" -d "$(($deadline_us/1000))" -c "$con" --rps "$rps" -P "30" "http://$hostname:10030/fib" > "$results_directory/con$con.txt" || { #-n "$iterations"
			printf "[ERR]\n"
			panic "experiment failed"
			return 1
		}
		get_result_count "$results_directory/con$con.txt" || {
			printf "[ERR]\n"
			panic "con$con unexpectedly has zero requests"
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

		if [[ ! -f "$results_directory/con$con.txt" ]]; then
			printf "[ERR]\n"
			error_msg "Missing $results_directory/con$con.txt"
			return 1
		fi

		# Get Number of 200s and then calculate Success Rate (percent of requests that return 200)
		#  If using loadtest -n option (not -t), then use ok/iterations instead of ok/total.
		total=$(grep "Completed requests:" "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 14)
		missed=$(grep "Total errors:" "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 13)
		ok=$((total-missed))
		((ok == 0)) && continue # If all errors, skip line
		success_rate=$(echo "scale=2; $ok/$total*100"|bc)
		printf "%d,%3.1f\n" "$con" "$success_rate" >> "$results_directory/success.csv"

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(grep "Requests per second" "$results_directory/con$con.txt" | cut -d ' ' -f 14 | tail -n 1)
		printf "%d,%d\n" "$con" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data
		min=0
		p50=$(echo 1000*"$(grep 50% "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p90=$(echo 1000*"$(grep 90% "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p99=$(echo 1000*"$(grep 99% "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p100=$(echo 1000*"$(grep 100% "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 12 | tail -n 1)" | bc)
		mean=$(echo 1000*"$(grep "Mean latency:" "$results_directory/con$con.txt" | tr -s ' ' | cut -d ' ' -f 13)" | bc)

		printf "%d,%d,%d,%.2f,%d,%d,%d,%d\n" "$con" "$ok" "$min" "$mean" "$p50" "$p90" "$p99" "$p100" >> "$results_directory/latency.csv"
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
