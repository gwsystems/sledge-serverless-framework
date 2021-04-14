#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate

declare -gi iterations=10000
declare -ga concurrency=(1 20 40 60 80 100)

run_samples() {
	local hostname="${1:-localhost}"

	# Scrape the perf window size from the source if possible
	local -r perf_window_path="../../include/perf_window.h"
	local -i perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		echo "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h"
		echo "Defaulting to 16"
		perf_window_buffer_size=16
	fi
	local -ir perf_window_buffer_size

	printf "Running Samples: "
	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -q 200 -cpus 3 -o csv -m GET "http://${hostname}:10000" 1> /dev/null 2> /dev/null || {
		printf "[ERR]\n"
		error_msg "samples failed"
		return 1
	}

	echo "[OK]"
	return 0
}

# Execute the experiments
# $1 (results_directory) - a directory where we will store our results
# $2 (hostname="localhost") - an optional parameter that sets the hostname. Defaults to localhost
run_experiments() {
	if (($# < 1 || $# > 2)); then
		error_msg "invalid number of arguments \"$1\""
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory \"$1\" does not exist"
		return 1
	fi

	local results_directory="$1"
	local hostname="${2:-localhost}"

	# Execute the experiments
	echo "Running Experiments"
	for conn in ${concurrency[*]}; do
		printf "\t%d Concurrency: " "$conn"
		hey -n "$iterations" -c "$conn" -cpus 2 -o csv -m GET "http://$hostname:10000" > "$results_directory/con$conn.csv" 2> /dev/null
		echo "[OK]"
	done
}

process_results() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	local -r results_directory="$1"

	echo -n "Processing Results: "

	# Write headers to CSVs
	printf "Concurrency,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Concurrency,Throughput\n" >> "$results_directory/throughput.csv"
	printf "Con,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	for conn in ${concurrency[*]}; do
		# Calculate Success Rate for csv (percent of requests resulting in 200)
		awk -F, '
		$7 == 200 {ok++}
		END{printf "'"$conn"',%3.5f\n", (ok / '"$iterations"' * 100)}
	' < "$results_directory/con$conn.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to ms, and sort
		awk -F, '$7 == 200 {print ($1 * 1000)}' < "$results_directory/con$conn.csv" \
			| sort -g > "$results_directory/con$conn-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/con$conn-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic duration_sec value?
		duration=$(tail -n1 "$results_directory/con$conn.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%d,%f\n" "$conn" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		awk '
		BEGIN {
			sum = 0
			p50 = int('"$oks"' * 0.5)
			p90 = int('"$oks"' * 0.9)
			p99 = int('"$oks"' * 0.99)
			p100 = '"$oks"'
			printf "'"$conn"',"
		}
		NR==p50 {printf "%1.4f,", $0}
		NR==p90 {printf "%1.4f,", $0}
		NR==p99 {printf "%1.4f,", $0}
		NR==p100 {printf "%1.4f\n", $0}
	' < "$results_directory/con$conn-response.csv" >> "$results_directory/latency.csv"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/con$conn-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	for file in success latency throughput; do
		echo -n "#" > "$results_directory/$file.dat"
		tr ',' ' ' < "$results_directory/$file.csv" | column -t >> "$results_directory/$file.dat"
	done

	# Generate gnuplots
	generate_gnuplots latency success throughput

	# Cleanup, if requires
	echo "[OK]"

}

main "$@"
