#!/bin/bash

# Needed for trimming trailing "deadline description" suffix. lpd_1.8 -> lpd
shopt -s extglob

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
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source validate_dependencies.sh || exit 1
source percentiles_table.sh || exit 1

validate_dependencies hey jq

declare -a workloads=()
declare -A port=()

# Example of stripping off multiple suffix from variable in format string_multiple
# test="ekf_12223.23343"
# ${test%%_+([[:digit:]]).+([[:digit:]])}
declare -Ar body=(
	[fib10]="-d 10\n"
	[fib10_rich]="-d 10\n"
	[fib40]="-d 40\n"
)

# The deadlines for each of the payloads
# TODO: Scrape these from spec.json
declare -Ar deadlines_us=(
	[fib10]=600
	[fib10_rich]=600
	[fib40]=771200
)

initialize_globals() {
	while read -r line; do
		# Read into buffer array, splitting on commas
		readarray -t -d, buffer < <(echo -n "$line")
		workload="${buffer[1]}"
		# Update workload mix structures
		workloads+=("$workload")
		port+=(["$workload"]=$(get_port "$workload"))
	done < "$__run_sh__base_path/mix.csv"
}

get_port() {
	local name="$1"
	{
		echo "["
		cat ./spec.json
		echo "]"
	} | jq ".[] | select(.name == \"$name\") | .port"
}

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

	local -A floor=()
	local -A length=()
	local -i total=0

	local -a buffer=()
	local workload=""
	local -i odds=0

	# Update workload mix structures
	while read -r line; do
		# Read into buffer array, splitting on commas
		readarray -t -d, buffer < <(echo -n "$line")
		# Use human friendly names
		odds="${buffer[0]}"
		workload="${buffer[1]}"
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
	local -ir total_iterations=500 #10000
	local -ir worker_max=50
	local pids

	printf "Running Experiments: "

	# Select a random workload using the workload mix and run command, writing output to disk
	for ((i = 0; i < total_iterations; i += batch_size)); do
		# Block waiting for a worker to finish if we are at our max
		while (($(pgrep --count hey) >= worker_max)); do
			#shellcheck disable=SC2046
			wait -n $(pgrep hey | tr '\n' ' ')
		done
		roll=$((RANDOM % total))
		((batch_id++))
		for workload in "${workloads[@]}"; do
			shortname="${workload%%_+([[:digit:]]).+([[:digit:]])}"
			if ((roll >= floor[$workload] && roll < floor[$workload] + length[$workload])); then
				# We require word splitting on the value returned by the body associative array
				#shellcheck disable=SC2086
				hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m GET ${body[$shortname]} "http://${hostname}:${port[$workload]}" | tail -n1 >> "$results_directory/$workload.csv" 2> /dev/null &
				break
			fi
		done
	done
	pids=$(pgrep hey | tr '\n' ' ')
	[[ -n $pids ]] && wait -f $pids
	printf "[OK]\n"

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

	printf "Processing Client Results: "

	# Write headers to CSVs
	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv"


	for workload in "${workloads[@]}"; do
		local -i deadline=${deadlines_us[${workload/}]}

		# Calculate Success Rate for csv (percent of requests that return 200 within deadline)
		awk -F, '
			$7 == 200 && ($1 * 1000000) <= '"$deadline"' {ok++}
			END{printf "'"$workload"',%3.5f\n", (ok / NR * 100)}
		' < "$results_directory/$workload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to ms, and sort
		awk -F, '$7 == 200 {print ($1 * 1000000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/$workload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# We determine duration by looking at the timestamp of the last complete request
		# TODO: Should this instead just use the client-side synthetic duration_sec value?
		duration=$(tail -n1 "$results_directory/$workload.csv" | cut -d, -f8)

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(echo "$oks/$duration" | bc)
		printf "%s,%f\n" "$workload" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency.csv" "$workload"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$workload-response.csv"
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

process_server_results() {
	local -r results_directory="${1:?results_directory not set}"

	if ! [[ -d "$results_directory" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	printf "Processing Server Results: "

	# Write headers to CSVs
	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	#printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv"

	local -a metrics=(total queued initializing runnable running blocked returned)

	local -A fields=(
		[total]=6
		[queued]=7
		[initializing]=8
		[runnable]=9
		[running]=10
		[blocked]=11
		[returned]=12
	)

	# Write headers to CSVs
	for metric in "${metrics[@]}"; do
		percentiles_table_header "$results_directory/$metric.csv" "module"
	done
	percentiles_table_header "$results_directory/memalloc.csv" "module"

	for workload in "${workloads[@]}"; do
		mkdir "$results_directory/$workload"

		local -i deadline=${deadlines_us[${workload/}]}

		# TODO: Only include Complete
		for metric in "${metrics[@]}"; do
			awk -F, '$2 == "'"$workload"'" {printf("%.4f\n", $'"${fields[$metric]}"' / $13)}' < "$results_directory/perf.log" | sort -g > "$results_directory/$workload/${metric}_sorted.csv"

			percentiles_table_row "$results_directory/$workload/${metric}_sorted.csv" "$results_directory/${metric}.csv" "$workload"

			# Delete scratch file used for sorting/counting
			# rm -rf "$results_directory/$workload/${metric}_sorted.csv"
		done

		# Memory Allocation
		awk -F, '$2 == "'"$workload"'" {printf("%.0f\n", $14)}' < "$results_directory/perf.log" | sort -g > "$results_directory/$workload/memalloc_sorted.csv"

		percentiles_table_row "$results_directory/$workload/memalloc_sorted.csv" "$results_directory/memalloc.csv" "$workload" "%1.0f"


		# Calculate Success Rate for csv (percent of requests that complete), $1 and deadline are both in us, so not converting 
		awk -F, '
			$1 <= '"$deadline"' {ok++}
			END{printf "'"$workload"',%3.5f\n", (ok / NR * 100)}
		' < "$results_directory/$workload/total_sorted.csv" >> "$results_directory/success.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload/total_sorted.csv" "$results_directory/latency.csv" "$workload"

		# Delete scratch file used for sorting/counting
		# rm -rf "$results_directory/$workload/memalloc_sorted.csv"

		# Delete directory
		# rm -rf "${results_directory:?}/${workload:?}"

	done

	# Transform csvs to dat files for gnuplot
	for metric in "${metrics[@]}"; do
		csv_to_dat "$results_directory/$metric.csv"
	done
	csv_to_dat "$results_directory/memalloc.csv"
	csv_to_dat "$results_directory/success.csv" "$results_directory/latency.csv"

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

# Client Code
# Expected Symbol used by the framework
experiment_client() {
	local -r target_hostname="$1"
	local -r results_directory="$2"

	run_experiments "$target_hostname" "$results_directory" || return 1
	process_client_results "$results_directory" || return 1

	return 0
}

initialize_globals
framework_init "$@"
