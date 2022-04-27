#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Success - The percentage of requests that complete by their deadlines
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

validate_dependencies hey jq

# The four types of results that we are capturing.
declare -ar workloads=(fib30 fib30_rich)

# The deadlines for each of the workloads
# TODO: Scrape these from spec.json
declare -Ar deadlines_us=(
	[fib30]=10000
	[fib30_rich]=10000
)


# Execute the experiments sequentially and concurrently
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
	# local -ir duration_sec=5

	# The duration in seconds that the low priority task should run before the high priority task starts
	# local -ir offset=5

	printf "Running Experiments\n"

	# Run concurrently
	# The lower priority has offsets to ensure it runs the entire time the high priority is trying to run
	# This asynchronously trigger jobs and then wait on their pids
	local fib30_rich_PID
	local fib30_PID

	hey -disable-compression -disable-keepalive -disable-redirects -z 10s -n 5400 -c 54 -t 0 -o csv -m GET -d "30\n" "http://${hostname}:10030" > "$results_directory/fib30.csv" 2> /dev/null &
	fib30_PID="$!"

	sleep 3s	
	
	hey -disable-compression -disable-keepalive -disable-redirects -z 5s -n 5400 -c 9 -t 0 -o csv -m GET -d "30\n" "http://${hostname}:20030" > "$results_directory/fib30_rich.csv" 2> /dev/null &
	fib30_rich_PID="$!"

	wait -f "$fib30_rich_PID" || {
		printf "\tfib30_rich: [ERR]\n"
		panic "failed to wait -f ${fib30_rich_PID}"
		return 1
	}
	get_result_count "$results_directory/fib30_rich.csv" || {
		printf "\tfib30_rich: [ERR]\n"
		panic "fib30_rich has zero requests."
		return 1
	}
	printf "\tfib30_rich: [OK]\n"
	
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
	printf "\tfib30: [OK]\n"

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
	printf "workload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "workload,Throughput\n" >> "$results_directory/throughput.csv"
	percentiles_table_header "$results_directory/latency.csv"

	for workload in "${workloads[@]}"; do
		# Strip the  suffix when getting the deadline
		local -i deadline=${deadlines_us[${workload//}]}

		# Calculate Success Rate for csv (percent of requests that return 200 within deadline)
		awk -F, '
			$7 == 200 && ($1 * 1000000) <= '"$deadline"' {ok++}
			END{printf "'"$workload"',%3.5f\n", (ok / (NR - 1) * 100)}
		' < "$results_directory/$workload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convert from s to us, and sort
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
	# percentiles_table_header "$results_directory/latency.csv"

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
			# rm "$results_directory/$workload/${metric}_sorted.csv"
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
		# Not necessary, since it's the same thing as total.csv
		# percentiles_table_row "$results_directory/$workload/total_sorted.csv" "$results_directory/latency.csv" "$workload"

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
	csv_to_dat "$results_directory/success.csv" #"$results_directory/latency.csv"

	rm "$results_directory/memalloc.csv" "$results_directory/success.csv" #"$results_directory/latency.csv"

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

	run_experiments "$target_hostname" "$results_directory" || return 1
	process_client_results "$results_directory" || return 1

	return 0
}

framework_init "$@"
