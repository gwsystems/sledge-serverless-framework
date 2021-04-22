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
	local -r perf_window_path="$(path_join "$__run_sh__base_path" ../../include/perf_window.h)"
	local -i perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		printf "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h\n"
		printf "Defaulting to 16\n"
		perf_window_buffer_size=16
	fi
	local -ir perf_window_buffer_size

	printf "Running Samples: "
	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" || {
		printf "[ERR]\n"
		panic "fib40 samples failed with $?"
		return 1
	}

	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:100010" || {
		printf "[ERR]\n"
		panic "fib10 samples failed with $?"
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

	# The duration in seconds that we want the client to send requests
	local -ir duration_sec=15

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir offset=5

	printf "Running Experiments\n"

	# Run each separately
	printf "\tfib40: "
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "40\n" "http://$hostname:10040" > "$results_directory/fib40.csv" 2> /dev/null || {
		printf "[ERR]\n"
		panic "fib40 failed"
		return 1
	}
	get_result_count "$results_directory/fib40.csv" || {
		printf "[ERR]\n"
		panic "fib40 unexpectedly has zero requests"
		return 1
	}
	printf "[OK]\n"

	printf "\tfib10: "
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "10\n" "http://$hostname:10010" > "$results_directory/fib10.csv" 2> /dev/null || {
		printf "[ERR]\n"
		panic "fib10 failed"
		return 1
	}
	get_result_count "$results_directory/fib10.csv" || {
		printf "[ERR]\n"
		panic "fib10 unexpectedly has zero requests"
		return 1
	}
	printf "[OK]\n"

	# Run concurrently
	# The lower priority has offsets to ensure it runs the entire time the high priority is trying to run
	# This asynchronously trigger jobs and then wait on their pids
	local fib40_con_PID
	local fib10_con_PID

	hey -z $((duration_sec + 2 * offset))s -cpus 2 -c 100 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" > "$results_directory/fib40_con.csv" 2> /dev/null &
	fib40_con_PID="$!"

	sleep $offset

	hey -z "${duration_sec}s" -cpus 2 -c 100 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:10010" > "$results_directory/fib10_con.csv" 2> /dev/null &
	fib10_con_PID="$!"

	wait -f "$fib10_con_PID" || {
		printf "\tfib10_con: [ERR]\n"
		panic "failed to wait -f ${fib10_con_PID}"
		return 1
	}
	get_result_count "$results_directory/fib10_con.csv" || {
		printf "\tfib10_con: [ERR]\n"
		panic "fib10_con has zero requests. This might be because fib40_con saturated the runtime"
		return 1
	}
	printf "\tfib10_con: [OK]\n"

	wait -f "$fib40_con_PID" || {
		printf "\tfib40_con: [ERR]\n"
		panic "failed to wait -f ${fib40_con_PID}"
		return 1
	}
	get_result_count "$results_directory/fib40_con.csv" || {
		printf "\tfib40_con: [ERR]\n"
		panic "fib40_con has zero requests."
		return 1
	}
	printf "\tfib40_con: [OK]\n"

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
	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	# The four types of results that we are capturing.
	# fib10 and fib 40 are run sequentially.
	# fib10_con and fib40_con are run concurrently
	local -ar payloads=(fib10 fib10_con fib40 fib40_con)

	# The deadlines for each of the workloads
	# TODO: Scrape these from spec.json
	local -Ar deadlines_ms=(
		[fib10]=2
		[fib40]=3000
	)

	for payload in "${payloads[@]}"; do
		# Strip the _con suffix when getting the deadline
		local -i deadline=${deadlines_ms[${payload/_con/}]}

		# Calculate Success Rate for csv (percent of requests that return 200 within deadline)
		awk -F, '
			$7 == 200 && ($1 * 1000) <= '"$deadline"' {ok++}
			END{printf "'"$payload"',%3.5f\n", (ok / (NR - 1) * 100)}
		' < "$results_directory/$payload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to ms, and sort
		awk -F, '$7 == 200 {print ($1 * 1000)}' < "$results_directory/$payload.csv" \
			| sort -g > "$results_directory/$payload-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/$payload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic duration_sec value?
		duration=$(tail -n1 "$results_directory/$payload.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%s,%f\n" "$payload" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		awk '
			BEGIN {
				sum = 0
				p50 = int('"$oks"' * 0.5)
				p90 = int('"$oks"' * 0.9)
				p99 = int('"$oks"' * 0.99)
				p100 = '"$oks"'
				printf "'"$payload"',"
			}
			NR==p50  {printf "%1.4f,",  $0}
			NR==p90  {printf "%1.4f,",  $0}
			NR==p99  {printf "%1.4f,",  $0}
			NR==p100 {printf "%1.4f\n", $0}
		' < "$results_directory/$payload-response.csv" >> "$results_directory/latency.csv"

		# Delete scratch file used for sorting/counting
		# rm -rf "$results_directory/$payload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/throughput.csv" "$results_directory/latency.csv"

	# Generate gnuplots. Commented out because we don't have *.gnuplots defined
	# generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
	# 	printf "[ERR]\n"
	# 	panic "failed to generate gnuplots"
	# }

	printf "[OK]\n"
	return 0
}

# Expected Symbol used by the framework
experiment_main() {
	local -r target_hostname="$1"
	local -r results_directory="$2"

	run_samples "$target_hostname" || return 1
	run_experiments "$target_hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1

	return 0
}

main "$@"
