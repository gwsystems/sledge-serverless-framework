#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Success - The percentage of requests that complete out of the total expected
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (unit?) of successful requests at the p50, p90, p99, and p100 percentiles

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

validate_dependencies loadtest gnuplot

# The global configs for the scripts
declare -gi iterations=10000
declare -gi duration_sec=60
declare -g using_rps=true # should mostly be true 
declare -ga concurrency=(1 9 18 20 30 40 60 80 100)
declare -gi deadline_ms=10 #10ms for fib30

run_samples() {
	if (($# != 1)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	fi

	local hostname="$1"

	# Scrape the perf window size from the source if possible
	# TODO: Make a util function
	local -r perf_window_path="$(path_join "$__run_sh__base_path" ../../include/perf_window_t.h)"
	local -i perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		printf "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h\n"
		printf "Defaulting to 16\n"
		perf_window_buffer_size=16
	fi
	local -ir perf_window_buffer_size

	printf "Running Samples: "
	loadtest -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" --rps 216 -P "30" "http://${hostname}:10030" 1> /dev/null 2> /dev/null || {
		printf "[ERR]\n"
		panic "samples failed"
		return 1
	}

	printf "[OK]\n"
	return 0
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


	printf "Running Experiments:\n"
	for conn in "${concurrency[@]}"; do
		printf "\t%d Concurrency: " "$conn"
		rps=0
		if [ "$using_rps" = true ]; then 
			rps=$(echo 1000/$deadline_ms*"$conn" | bc)
		fi
		loadtest -t "$duration_sec" -c "$conn" --rps "$rps" -P "30" "http://$hostname:10030" > "$results_directory/con$conn.txt" || { #-n "$iterations"
			printf "[ERR]\n"
			panic "experiment failed"
			return 1
		}
		get_result_count "$results_directory/con$conn.txt" || {
			printf "[ERR]\n"
			panic "con$conn.txt unexpectedly has zero requests"
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

	for conn in "${concurrency[@]}"; do

		if [[ ! -f "$results_directory/con$conn.txt" ]]; then
			printf "[ERR]\n"
			error_msg "Missing $results_directory/con$conn.txt"
			return 1
		fi

		# Get Number of 200s and then calculate Success Rate (percent of requests that return 200)
		# P.S. The following makes sense only when using loadtest -t AND --rps optios together
		#  So, if using just loadtest -t without --rps, then success_rate result is meaningless
		#  If using loadtest -n option (not -t), then use ok/iterations instead of ok/total.
		ok=$(grep "Completed requests:" "$results_directory/con$conn.txt" | cut -d ' ' -f 14)
		total=$(echo 1000/$deadline_ms*"$conn"*$duration_sec | bc) #total = rps*duration_sec
		((ok == 0)) && continue # If all errors, skip line
		success_rate=$(echo "scale=2; $ok/$total*100"|bc)
		printf "%d,%3.2f\n", "$conn" "$success_rate" >> "$results_directory/success.csv"

		# Throughput is calculated as the mean number of successful requests per second
		throughput=$(grep "Requests per second" "$results_directory/con$conn.txt" | cut -d ' ' -f 14 | tail -n 1)
		printf "%d,%d\n" "$conn" "$throughput" >> "$results_directory/throughput.csv"

		# Generate Latency Data
		min=0
		p50=$(echo 1000*"$(grep 50% "$results_directory/con$conn.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p90=$(echo 1000*"$(grep 90% "$results_directory/con$conn.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p99=$(echo 1000*"$(grep 99% "$results_directory/con$conn.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		p100=$(echo 1000*"$(grep 100% "$results_directory/con$conn.txt" | tr -s ' ' | cut -d ' ' -f 12)" | bc)
		mean=$(echo 1000*"$(grep "Mean latency:" "$results_directory/con$conn.txt" | tr -s ' ' | cut -d ' ' -f 13)" | bc)

		printf "%d,%d,%d,%.2f,%d,%d,%d,%d\n", "$conn" "$ok" "$min" "$mean" "$p50" "$p90" "$p99" "$p100" >> "$results_directory/latency.csv"
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
	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	# printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	# percentiles_table_header "$results_directory/latency.csv"

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

	for workload in "${workloads[@]}"; do
		mkdir "$results_directory/$workload"

		local -i deadline=${deadlines_us[${workload/}]}

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
