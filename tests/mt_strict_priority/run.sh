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

validate_dependencies hey gnuplot

declare -gi iterations=10000
declare -gi duration_sec=60
declare -ga concurrency=(1 4 9 12 15 18 20 40)
# declare -gi deadline_us=10000 #10ms for fib30

# The deadlines for each of the workloads
declare -Ar deadlines_us=(
	[fib30_ng]=10000
	[fib30_g]=50000
)

# The types of results that we are capturing.
declare -ar workloads=(fib30_ng fib30_g)

# Not using this now
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
	hey -disable-compression -disable-keepalive -disable-redirects -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -q 200 -cpus 3 -o csv -m GET "http://${hostname}:10030" 1> /dev/null 2> /dev/null || {
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
	local fib30_ng_PID
	local fib30_g_PID

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir offset=1

	printf "Running Experiments:\n"
	# printf "\t Started fib30_g1 with con 9: \n" 

	# hey -disable-compression -disable-keepalive -disable-redirects -z "$((($duration_sec+$offset)*${#concurrency[@]}))"s -n "$iterations" -c 9 -o csv -m GET -d "30\n" "http://$hostname:10030" > "$results_directory/fib30_g1.csv" 2> /dev/null &
	# fib30_ng_PID="$!"

	for con in "${concurrency[@]}"; do
		printf "\t%d Concurrency: " "$con"

		hey -disable-compression -disable-keepalive -disable-redirects -z "$(($duration_sec+$offset))"s -n "$iterations" -c 9 -o csv -m GET -d "30\n" "http://$hostname:20030" > "$results_directory/fib30_g_con$con.csv" 2> /dev/null &
		fib30_g_PID="$!"

		sleep "$offset"s
		
		hey -disable-compression -disable-keepalive -disable-redirects -z "$duration_sec"s -n "$iterations" -c "$con" -o csv -m GET -d "30\n" "http://$hostname:10030" > "$results_directory/fib30_ng_con$con.csv" 2> /dev/null &
		fib30_ng_PID="$!"

		wait -f "$fib30_ng_PID" || {
			printf "\tfib30_ng: [ERR]\n"
			panic "failed to wait -f ${fib30_ng_PID}"
			return 1
		}
		get_result_count "$results_directory/fib30_ng_con$con.csv" || {
			printf "\tfib30_ng: [ERR]\n"
			panic "fib30_ng has zero requests."
			return 1
		}

		wait -f "$fib30_g_PID" || {
			printf "\tfib30_g_con$con: [ERR]\n"
			panic "failed to wait -f ${fib30_g_PID}"
			return 1
		}
		get_result_count "$results_directory/fib30_g_con$con.csv" || {
			printf "\tfib30_g_con$con: [ERR]\n"
			panic "fib30_g_con$con has zero requests."
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

	local -r results_directory_root="$1"

	printf "Processing Results: "

	for workload in "${workloads[@]}"; do
		mkdir "$results_directory_root/$workload" 
		local results_directory="$results_directory_root/$workload"
		local -i deadline_us=${deadlines_us[${workload}]}
		
		# Write headers to CSVs
		printf "Concurrency,${workload}_SR\n" >> "$results_directory/success.csv"
		printf "Concurrency,Throughput\n" >> "$results_directory/throughput.csv"
		percentiles_table_header "$results_directory/latency.csv" "Con"

		for con in "${concurrency[@]}"; do

			if [[ ! -f "$results_directory_root/${workload}_con$con.csv" ]]; then
				printf "[ERR]\n"
				error_msg "Missing $results_directory_root/${workload}_con$con.csv"
				return 1
			fi

			# Calculate Success Rate for csv (percent of requests that return 200)
			# P.S. When using hey -z option, this result is meaningless
			awk -F, '
			$7 == 200 && ($1 * 1000000) <= '"$deadline_us"' {ok++}
			END{printf "'"$con"',%3.1f\n", (ok / NR * 100)}
		' < "$results_directory_root/${workload}_con$con.csv" >> "$results_directory/success.csv"

			# Filter on 200s, convert from s to us, and sort
			awk -F, '$7 == 200 {print ($1 * 1000000)}' < "$results_directory_root/${workload}_con$con.csv" \
				| sort -g > "$results_directory/con$con-response.csv"

			# Get Number of 200s
			oks=$(wc -l < "$results_directory/con$con-response.csv")
			((oks == 0)) && continue # If all errors, skip line

			# We determine duration by looking at the timestamp of the last complete request
			# TODO: Should this instead just use the client-side synthetic duration_sec value?
			duration=$(tail -n1 "$results_directory_root/${workload}_con$con.csv" | cut -d, -f8)

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
	done

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
