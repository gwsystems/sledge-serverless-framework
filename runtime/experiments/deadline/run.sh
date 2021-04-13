#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Use -d flag if running under gdb
# TODO: GDB? Debug?
usage() {
	echo "$0 [options...]"
	echo ""
	echo "Options:"
	echo "  -t,--target=<target url> Execute as client against remote URL"
	echo "  -s,--serve=<EDF|FIFO>    Serve with scheduling policy, but do not run client"
}

initialize_globals() {
	# timestamp is used to name the results directory for a particular test run
	# shellcheck disable=SC2155
	declare -gir timestamp=$(date +%s)

	# shellcheck disable=SC2155
	declare -gr experiment_directory=$(pwd)

	# shellcheck disable=SC2155
	declare -gr binary_directory=$(cd ../../bin && pwd)

	# Scrape the perf window size from the source if possible
	declare -gr perf_window_path="../../include/perf_window.h"
	declare -gi perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		echo "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h"
		echo "Defaulting to 16"
		declare -ir perf_window_buffer_size=16
	fi

	declare -gx target=""
	declare -gx policy=""
	declare -gx role="both"
}

parse_arguments() {
	for i in "$@"; do
		case $i in
			-t=* | --target=*)
				if [[ "$role" == "server" ]]; then
					echo "Cannot set target when server"
					usage
					exit 1
				fi
				role=client
				target="${i#*=}"
				shift # past argument=value
				;;
			-s=* | --serve=*)
				if [[ "$role" == "client" ]]; then
					echo "Cannot serve with target is set"
					usage
					exit 1
				fi
				role=server
				policy="${i#*=}"
				if [[ ! $policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$policy\" is not a valid policy. EDF or FIFO allowed"
					usage
					exit 1
				fi
				shift # past argument=value
				;;
			-h | --help)
				usage
				;;
			*)
				echo "$1 is a not a valid option"
				usage
				exit 1
				;;
		esac
	done

	# Set globals as read only
	declare -r target
	declare -r policy
	declare -r role
}

start_runtime() {
	if (($# != 2)); then
		echo "${FUNCNAME[0]} error: invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		echo "${FUNCNAME[0]} error: expected EDF or FIFO was \"$1\""
		return 1
	elif ! [[ -d "$2" ]]; then
		echo "${FUNCNAME[0]} error: \"$2\" does not exist"
		return 1
	fi

	local -r scheduler="$1"
	local -r results_directory="$2"

	local -r log_name=log.txt
	local log="$results_directory/${log_name}"

	log_environment >> "$log"

	SLEDGE_NWORKERS=5 SLEDGE_SCHEDULER=$scheduler PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" >> "$log" 2>> "$log" &
	return $?
}

# Seed enough work to fill the perf window buffers
run_samples() {
	local hostname="${1:-localhost}"

	echo -n "Running Samples: "
	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" || {
		echo "error"
		return 1
	}

	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:100010" || {
		echo "error"
		return 1
	}

	return 0
}

# $1 (results_directory) - a directory where we will store our results
# $2 (hostname="localhost") - an optional parameter that sets the hostname. Defaults to localhost
run_experiments() {
	if (($# < 1 || $# > 2)); then
		echo "${FUNCNAME[0]} error: invalid number of arguments \"$1\""
		exit
	elif ! [[ -d "$1" ]]; then
		echo "${FUNCNAME[0]} error: \"$2\" does not exist"
		exit
	elif (($# > 2)) && [[ ! $1 =~ ^(EDF|FIFO)$ ]]; then
		echo "${FUNCNAME[0]} error: expected EDF or FIFO was \"$1\""
		exit
	fi

	local results_directory="$1"
	local hostname="${2:-localhost}"

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir offset=5

	# The duration in seconds that we want the client to send requests
	local -ir duration_sec=15

	echo "Running Experiments"

	# Run each separately
	echo "Running fib40"
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" > "$results_directory/fib40.csv" || {
		echo "error"
		return 1
	}
	get_result_count "$results_directory/fib40.csv" || {
		echo "fib40 unexpectedly has zero requests"
		return 1
	}

	echo "Running fib10"
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:10010" > "$results_directory/fib10.csv" || {
		echo "error"
		return 1
	}
	get_result_count "$results_directory/fib10.csv" || {
		echo "fib10 unexpectedly has zero requests"
		return 1
	}

	# Run concurrently
	# The lower priority has offsets to ensure it runs the entire time the high priority is trying to run
	# This asynchronously trigger jobs and then wait on their pids
	local -a pids=()

	echo "Running fib40_con"
	hey -z $((duration_sec + 2 * offset))s -cpus 2 -c 100 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" > "$results_directory/fib40_con.csv" &
	pids+=($!)

	sleep $offset

	echo "Running fib10_con"
	hey -z "${duration_sec}s" -cpus 2 -c 100 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:10010" > "$results_directory/fib10_con.csv" &
	pids+=($!)

	for ((i = 0; i < "${#pids[@]}"; i++)); do
		wait -n "${pids[@]}" || {
			echo "error"
			return 1
		}
	done

	get_result_count "$results_directory/fib40_con.csv" || {
		echo "fib40_con unexpectedly has zero requests"
		return 1
	}
	get_result_count "$results_directory/fib10_con.csv" || {
		echo "fib10_con has zero requests. This might be because fib40_con saturated the runtime"
	}

	return 0
}

process_results() {
	if (($# != 1)); then
		echo "${FUNCNAME[0]} error: invalid number of arguments \"$1\""
		exit
	elif ! [[ -d "$1" ]]; then
		echo "${FUNCNAME[0]} error: \"$1\" does not exist"
		exit
	fi

	local -r results_directory="$1"

	echo -n "Processing Results: "

	# Write headers to CSVs
	printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
	printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
	printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	# The four types of results that we are capturing.
	# fib10 and fib 40 are run sequentially.
	# fib10_con and fib40_con are run concurrently
	local -ar payloads=(fib10 fib10_con fib40 fib40_con)

	# The deadlines for each of the workloads
	local -Ar deadlines_ms=(
		[fib10]=2
		[fib40]=3000
	)

	for payload in "${payloads[@]}"; do
		# Strip the _con suffix when getting the deadline
		local -i deadline=${deadlines_ms[${payload/_con/}]}

		# Get Number of Requests, subtracting the header
		local -i requests=$(($(wc -l < "$results_directory/$payload.csv") - 1))
		((requests == 0)) && {
			echo "$payload unexpectedly has zero requests"
			continue
		}

		# Calculate Success Rate for csv
		awk -F, '
			$7 == 200 && ($1 * 1000) <= '"$deadline"' {ok++}
			END{printf "'"$payload"',%3.5f\n", (ok / (NR - 1) * 100)}
		' < "$results_directory/$payload.csv" >> "$results_directory/success.csv"

		# Filter on 200s, convery from s to ms, and sort
		awk -F, '$7 == 200 {print ($1 * 1000)}' < "$results_directory/$payload.csv" \
			| sort -g > "$results_directory/$payload-response.csv"

		# Get Number of 200s
		oks=$(wc -l < "$results_directory/$payload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# Get Latest Timestamp
		duration=$(tail -n1 "$results_directory/$payload.csv" | cut -d, -f8)
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
	# generate_gnuplots
}

run_server() {
	if (($# != 2)); then
		echo "${FUNCNAME[0]} error: invalid number of arguments \"$1\""
		exit
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		echo "${FUNCNAME[0]} error: expected EDF or FIFO was \"$1\""
		exit
	elif ! [[ -d "$2" ]]; then
		echo "${FUNCNAME[0]} error: \"$2\" does not exist"
		exit
	fi

	local -r scheduler="$1"
	local -r results_directory="$2"

	start_runtime "$scheduler" "$log" || {
		echo "${FUNCNAME[0]} error"
		return 1
	}
}

run_client() {
	results_directory="$experiment_directory/res/$timestamp"
	mkdir -p "$results_directory"

	run_samples "$target" || {
		echo "${FUNCNAME[0]} error"
		exit 1
	}

	sleep 5

	run_experiments "$target" || {
		echo "${FUNCNAME[0]} error"
		exit 1
	}

	sleep 1

	process_results "$results_directory" || {
		echo "${FUNCNAME[0]} error"
		exit 1
	}

	echo "[DONE]"
	exit 0

}

run_both() {
	local -ar schedulers=(EDF FIFO)
	for scheduler in "${schedulers[@]}"; do
		results_directory="$experiment_directory/res/$timestamp/$scheduler"
		mkdir -p "$results_directory"
		start_runtime "$scheduler" "$results_directory" || {
			echo "${FUNCNAME[0]} Error"
			exit 1
		}

		sleep 1

		run_samples || {
			echo "${FUNCNAME[0]} Error"
			kill_runtime
			exit 1
		}

		sleep 1

		run_experiments "$results_directory" || {
			echo "${FUNCNAME[0]} Error"
			kill_runtime
			exit 1
		}

		sleep 1
		kill_runtime || {
			echo "${FUNCNAME[0]} Error"
			exit 1
		}

		process_results "$results_directory" || {
			echo "${FUNCNAME[0]} Error"
			exit 1
		}

		echo "[DONE]"
		exit 0
	done
}

main() {
	initialize_globals
	parse_arguments "$@"

	echo "$timestamp"

	echo "Target: $target"
	echo "Policy: $policy"
	echo "Role: $role"

	case $role in
		both)
			run_both
			;;
		server)
			results_directory="$experiment_directory/res/$timestamp"
			mkdir -p "$results_directory"
			start_runtime "$target" "$results_directory"
			exit 0
			;;
		client) ;;
		*)
			echo "Invalid state"
			exit 1
			;;
	esac
}

main "$@"
