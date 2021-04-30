#!/bin/bash
# This experiment is intended to document how the level of concurrent requests influences
#   - latency
#   - throughput
#	- success/failure rate

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

# Source libraries from bash_libraries directory
source "path_join.sh" || exit 1
source "framework.sh" || exit 1
source "get_result_count.sh" || exit 1
source "generate_gnuplots.sh" || exit 1

if ! command -v hey > /dev/null; then
	echo "hey is not present."
	exit 1
fi

# Experiment Globals and Setups
declare -ar payloads=(1024 10240 102400 1048576)

declare -Ar ports=(
	[1024]=10000
	[10240]=10001
	[102400]=10002
	[1048576]=10003
)

declare -ri iterations=10000

# If the one of the expected body files doesn't exist, trigger the generation script.
cd "$__run_sh__base_path/body" && ./generate.sh && cd "$OLDPWD" || exit

run_samples() {
	local hostname="$1"

	# Scrape the perf window size from the source if possible
	local -r perf_window_path="$(path_join "$__run_sh__base_path" ../../include/perf_window.h)"
	local -i perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		printf "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h\n"
		printf "Defaulting to 16\n"
		perf_window_buffer_size=16
	fi
	local -ir perf_window_buffer_size

	# Execute workloads long enough for runtime to learn excepted execution time
	printf "Running Samples:\n"
	for payload in "${payloads[@]}"; do
		printf "\t%d Payload: " "$payload"
		hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -q 200 -o csv -m GET -D "$__run_sh__base_path/body/$payload.txt" "http://$hostname:${ports["$payload"]}" 1> /dev/null 2> /dev/null || {
			printf "[ERR]\n"
			panic "samples failed"
			return 1
		}
		printf "[OK]\n"
	done

	return 0
}

run_experiments() {
	if (($# != 2)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif [[ ! -d "$2" ]]; then
		panic "directory \"$2\" does not exist"
		return 1
	fi

	local hostname="$1"
	local results_directory="$2"

	# Execute the experiments
	printf "Running Experiments:\n"
	for payload in "${payloads[@]}"; do
		printf "\t%d Payload: " "$payload"
		hey -n "$iterations" -c 1 -cpus 2 -o csv -m GET -D "$__run_sh__base_path/body/$payload.txt" "http://$hostname:${ports["$payload"]}" > "$results_directory/$payload.csv" 2> /dev/null || {
			printf "[ERR]\n"
			panic "$payload experiment failed"
			return 1
		}
		get_result_count "$results_directory/$payload.csv" || {
			printf "[ERR]\n"
			panic "$payload.csv unexpectedly has zero requests"
			return 1
		}
		printf "[OK]\n"
	done

	return 0
}

process_results() {
	if (($# != 1)); then
		panic "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		panic "directory $1 does not exist"
		return 1
	fi

	local -r results_directory="$1"

	printf "Processing Results: "

	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	for payload in ${payloads[*]}; do
		# Calculate Success Rate for csv
		awk -F, '
			$7 == 200 {ok++}
			END{printf "'"$payload"',%3.5f\n", (ok / '"$iterations"' * 100)}
		' < "$results_directory/$payload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convery from s to ms, and sort
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
		printf "%d,%f\n" "$payload" "$throughput" >> "$results_directory/throughput.csv"

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
		NR==p50 {printf "%1.4f,", $0}
		NR==p90 {printf "%1.4f,", $0}
		NR==p99 {printf "%1.4f,", $0}
		NR==p100 {printf "%1.4f\n", $0}
	' < "$results_directory/$payload-response.csv" >> "$results_directory/latency.csv"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$payload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	for file in success latency throughput; do
		printf "#" > "$results_directory/$file.dat"
		tr ',' ' ' < "$results_directory/$file.csv" | column -t >> "$results_directory/$file.dat"
	done

	# Generate gnuplots
	generate_gnuplots "$results_directory" "$__run_sh__base_path" || {
		printf "[ERR]\n"
		panic "failed to generate gnuplots"
	}

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

# Delegating to main provided by framework
main "$@"
