#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Success - The percentage of requests that complete by their deadlines
# 	TODO: Does this handle non-200s?
# Throughput - The mean number of successful requests per second
# Latency - the rount-trip resonse time (unit?) of successful requests at the p50, p90, p99, and p100 percetiles

# Use -d flag if running under gdb
# TODO: Just use ENV for policy and other runtime dynamic variables?
usage() {
	echo "$0 [options...]"
	echo ""
	echo "Options:"
	echo "  -t,--target=<target url> Execute as client against remote URL"
	echo "  -s,--serve=<EDF|FIFO>    Serve with scheduling policy, but do not run client"
	echo "  -d,--debug=<EDF|FIFO>    Debug under GDB with scheduling policy, but do not run client"
	echo "  -p,--perf=<EDF|FIFO>     Run under perf with scheduling policy. Run on baremetal Linux host!"
}

# Declares application level global state
initialize_globals() {
	# timestamp is used to name the results directory for a particular test run
	# shellcheck disable=SC2155
	declare -gir timestamp=$(date +%s)

	# shellcheck disable=SC2155
	declare -gr experiment_directory=$(pwd)

	# shellcheck disable=SC2155
	declare -gr binary_directory=$(cd ../../bin && pwd)

	# Scrape the perf window size from the source if possible
	local -r perf_window_path="../../include/perf_window.h"
	declare -gi perf_window_buffer_size
	if ! perf_window_buffer_size=$(grep "#define PERF_WINDOW_BUFFER_SIZE" < "$perf_window_path" | cut -d\  -f3); then
		echo "Failed to scrape PERF_WINDOW_BUFFER_SIZE from ../../include/perf_window.h"
		echo "Defaulting to 16"
		declare -ir perf_window_buffer_size=16
	fi
	declare -gir perf_window_buffer_size

	# Globals used by parse_arguments
	declare -g target=""
	declare -g policy=""
	declare -g role=""

	# Configure environment variables
	export PATH=$binary_directory:$PATH
	export LD_LIBRARY_PATH=$binary_directory:$LD_LIBRARY_PATH
	export SLEDGE_NWORKERS=5
}

# Parses arguments from the user and sets associates global state
parse_arguments() {
	for i in "$@"; do
		case $i in
			-t=* | --target=*)
				if [[ "$role" == "server" ]]; then
					echo "Cannot set target when server"
					usage
					return 1
				fi
				role=client
				target="${i#*=}"
				shift
				;;
			-s=* | --serve=*)
				if [[ "$role" == "client" ]]; then
					echo "Cannot use -s,--serve with -t,--target"
					usage
					return 1
				fi
				role=server
				policy="${i#*=}"
				if [[ ! $policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$policy\" is not a valid policy. EDF or FIFO allowed"
					usage
					return 1
				fi
				shift
				;;
			-d=* | --debug=*)
				if [[ "$role" == "client" ]]; then
					echo "Cannot use -d,--debug with -t,--target"
					usage
					return 1
				fi
				role=debug
				policy="${i#*=}"
				if [[ ! $policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$policy\" is not a valid policy. EDF or FIFO allowed"
					usage
					return 1
				fi
				shift
				;;
			-p=* | --perf=*)
				if [[ "$role" == "perf" ]]; then
					echo "Cannot use -p,--perf with -t,--target"
					usage
					return 1
				fi
				role=perf
				policy="${i#*=}"
				if [[ ! $policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$policy\" is not a valid policy. EDF or FIFO allowed"
					usage
					return 1
				fi
				shift
				;;
			-h | --help)
				usage
				exit 0
				;;
			*)
				echo "$1 is a not a valid option"
				usage
				return 1
				;;
		esac
	done

	# default to both if no arguments were passed
	if [[ -z "$role" ]]; then
		role="both"
	fi

	# Set globals as read only
	declare -r target
	declare -r policy
	declare -r role
}

# Starts the Sledge Runtime
start_runtime() {
	printf "Starting Runtime: "
	if (($# != 2)); then
		printf "[ERR]\n"
		error_msg "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		error_msg "expected EDF or FIFO was \"$1\""
		return 1
	elif ! [[ -d "$2" ]]; then
		printf "[ERR]\n"
		error_msg "directory \"$2\" does not exist"
		return 1
	fi

	local -r scheduler="$1"
	local -r results_directory="$2"

	local -r log_name=log.txt
	local log="$results_directory/${log_name}"

	log_environment >> "$log"

	SLEDGE_SCHEDULER="$scheduler" \
		sledgert "$experiment_directory/spec.json" >> "$log" 2>> "$log" &

	printf "[OK]\n"
	return 0
}

# Sends requests until the per-module perf window buffers are full
# This ensures that Sledge has accurate estimates of execution time
run_samples() {
	local hostname="${1:-localhost}"

	echo -n "Running Samples: "
	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "40\n" "http://${hostname}:10040" 1> /dev/null 2> /dev/null || {
		error_msg "fib40 samples failed"
		return 1
	}

	hey -n "$perf_window_buffer_size" -c "$perf_window_buffer_size" -cpus 3 -t 0 -o csv -m GET -d "10\n" "http://${hostname}:100010" 1> /dev/null 2> /dev/null || {
		error_msg "fib10 samples failed"
		return 1
	}

	echo "[OK]"
	return 0
}

# Execute the fib10 and fib40 experiments sequentially and concurrently
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

	# The duration in seconds that we want the client to send requests
	local -ir duration_sec=15

	# The duration in seconds that the low priority task should run before the high priority task starts
	local -ir offset=5

	printf "Running Experiments\n"

	# Run each separately
	printf "\tfib40: "
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "40\n" "http://$hostname:10040" > "$results_directory/fib40.csv" 2> /dev/null || {
		printf "[ERR]\n"
		error_msg "fib40 failed"
		return 1
	}
	get_result_count "$results_directory/fib40.csv" || {
		printf "[ERR]\n"
		error_msg "fib40 unexpectedly has zero requests"
		return 1
	}
	printf "[OK]\n"

	printf "\tfib10: "
	hey -z ${duration_sec}s -cpus 4 -c 100 -t 0 -o csv -m GET -d "10\n" "http://$hostname:10010" > "$results_directory/fib10.csv" 2> /dev/null || {
		printf "[ERR]\n"
		error_msg "fib10 failed"
		return 1
	}
	get_result_count "$results_directory/fib10.csv" || {
		printf "[ERR]\n"
		error_msg "fib10 unexpectedly has zero requests"
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
		error_msg "failed to wait -f ${fib10_con_PID}"
		return 1
	}
	get_result_count "$results_directory/fib10_con.csv" || {
		printf "\tfib10_con: [ERR]\n"
		error_msg "fib10_con has zero requests. This might be because fib40_con saturated the runtime"
		return 1
	}
	printf "\tfib10_con: [OK]\n"

	wait -f "$fib40_con_PID" || {
		printf "\tfib40_con: [ERR]\n"
		error_msg "failed to wait -f ${fib40_con_PID}"
		return 1
	}
	get_result_count "$results_directory/fib40_con.csv" || {
		printf "\tfib40_con: [ERR]\n"
		error_msg "fib40_con has zero requests."
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
	# TODO: Scrape these from spec.json
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
	# generate_gnuplots
}

run_server() {
	if (($# != 1)); then
		error_msg "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		error_msg "expected EDF or FIFO was \"$1\""
		return 1
	fi

	local -r scheduler="$1"

	if [[ "$role" == "both" ]]; then
		local results_directory="$experiment_directory/res/$timestamp/$scheduler"
	elif [[ "$role" == "server" ]]; then
		local results_directory="$experiment_directory/res/$timestamp"
	else
		error_msg "Unexpected $role"
		return 1
	fi

	mkdir -p "$results_directory"

	start_runtime "$scheduler" "$results_directory" || {
		echo "start_runtime RC: $?"
		error_msg "Error calling start_runtime $scheduler $results_directory"
		return 1
	}

	return 0
}

run_perf() {
	if (($# != 1)); then
		printf "[ERR]\n"
		error_msg "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		error_msg "expected EDF or FIFO was \"$1\""
		return 1
	fi

	if ! command -v perf; then
		echo "perf is not present."
		exit 1
	fi

	local -r scheduler="$1"

	SLEDGE_SCHEDULER="$scheduler" perf record -g -s sledgert "$experiment_directory/spec.json"
}

# Starts the Sledge Runtime under GDB
run_debug() {
	# shellcheck disable=SC2155
	local project_directory=$(cd ../.. && pwd)
	if (($# != 1)); then
		printf "[ERR]\n"
		error_msg "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		error_msg "expected EDF or FIFO was \"$1\""
		return 1
	fi

	local -r scheduler="$1"

	if [[ "$project_directory" != "/sledge/runtime" ]]; then
		printf "It appears that you are not running in the container. Substituting path to match host environment\n"
		SLEDGE_SCHEDULER="$scheduler" gdb \
			--eval-command="handle SIGUSR1 nostop" \
			--eval-command="handle SIGPIPE nostop" \
			--eval-command="set pagination off" \
			--eval-command="set substitute-path /sledge/runtime $project_directory" \
			--eval-command="run $experiment_directory/spec.json" \
			sledgert
	else
		SLEDGE_SCHEDULER="$scheduler" gdb \
			--eval-command="handle SIGUSR1 nostop" \
			--eval-command="handle SIGPIPE nostop" \
			--eval-command="set pagination off" \
			--eval-command="run $experiment_directory/spec.json" \
			sledgert
	fi
	return 0
}

run_client() {
	if [[ "$role" == "both" ]]; then
		local results_directory="$experiment_directory/res/$timestamp/$scheduler"
	elif [[ "$role" == "client" ]]; then
		local results_directory="$experiment_directory/res/$timestamp"
	else
		error_msg "${FUNCNAME[0]} Unexpected $role"
		return 1
	fi

	mkdir -p "$results_directory"

	run_samples "$target" || {
		error_msg "Error calling run_samples $target"
		return 1
	}

	run_experiments "$results_directory" || {
		error_msg "Error calling run_experiments $results_directory"
		return 1
	}

	process_results "$results_directory" || {
		error_msg "Error calling process_results $results_directory"
		return 1
	}

	echo "[OK]"
	return 0
}

run_both() {
	local -ar schedulers=(EDF FIFO)
	for scheduler in "${schedulers[@]}"; do
		printf "Running %s\n" "$scheduler"

		run_server "$scheduler" || {
			error_msg "Error calling run_server"
			return 1
		}

		run_client || {
			error_msg "Error calling run_client"
			kill_runtime
			return 1
		}

		kill_runtime || {
			error_msg "Error calling kill_runtime"
			return 1
		}

	done

	return 0
}

main() {
	initialize_globals
	parse_arguments "$@" || {
		exit 1
	}

	case $role in
		both)
			run_both
			;;
		server)
			run_server "$policy"
			;;
		debug)
			run_debug "$policy"
			;;
		perf)
			run_perf "$policy"
			;;
		client)
			run_client
			;;
		*)
			echo "Invalid state"
			false
			;;
	esac

	exit "$?"
}

main "$@"
