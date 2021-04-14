#!/bin/bash

declare __common_did_dump_callstack=false

error_msg() {
	[[ "$__common_did_dump_callstack" == false ]] && {
		printf "%.23s %s() in %s, line %s: %s\n" "$(date +%F.%T.%N)" "${FUNCNAME[1]}" "${BASH_SOURCE[1]##*/}" "${BASH_LINENO[0]}" "${@}"
		__common_dump_callstack
		__common_did_dump_callstack=true
	}
}

__common_dump_callstack() {
	echo "Call Stack:"
	# Skip the dump_bash_stack and error_msg_frames
	for ((i = 2; i < ${#FUNCNAME[@]}; i++)); do
		printf "\t%d - %s\n" "$((i - 2))" "${FUNCNAME[i]}"
	done
}

log_environment() {
	if ! command -v git &> /dev/null; then
		echo "git could not be found"
		exit
	fi
	echo "*******"
	echo "* Git *"
	echo "*******"
	git log | head -n 1 | cut -d' ' -f2
	git status
	echo ""

	echo "************"
	echo "* Makefile *"
	echo "************"
	cat ../../Makefile
	echo ""

	echo "**********"
	echo "* Run.sh *"
	echo "**********"
	cat run.sh
	echo ""

	echo "************"
	echo "* Hardware *"
	echo "************"
	lscpu
	echo ""

	echo "*************"
	echo "* Execution *"
	echo "*************"
}

# Given a file, returns the number of results
# This assumes a *.csv file with a header
# $1 the file we want to check for results
# $2 an optional return nameref
get_result_count() {
	if (($# != 1)); then
		error_msg "insufficient parameters. $#/1"
		return 1
	elif [[ ! -f $1 ]]; then
		error_msg "the file $1 does not exist"
		return 1
	elif [[ ! -s $1 ]]; then
		error_msg "the file $1 is size 0"
		return 1
	fi

	local -r file=$1

	# Subtract one line for the header
	local -i count=$(($(wc -l < "$file") - 1))

	if (($# == 2)); then
		# shellcheck disable=2034
		local -n __result=$2
	fi

	if ((count > 0)); then
		return 0
	else
		return 1
	fi
}

usage() {
	echo "$0 [options...]"
	echo ""
	echo "Options:"
	echo "  -t,--target=<target url> Execute as client against remote URL"
	echo "  -s,--serve=<EDF|FIFO>    Serve with scheduling policy, but do not run client"
	echo "  -d,--debug=<EDF|FIFO>    Debug under GDB with scheduling policy, but do not run client"
	echo "  -p,--perf=<EDF|FIFO>     Run under perf with scheduling policy. Run on baremetal Linux host!"
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

# Declares application level global state
initialize_globals() {
	# timestamp is used to name the results directory for a particular test run
	# shellcheck disable=SC2155
	# shellcheck disable=SC2034
	declare -gir timestamp=$(date +%s)

	# shellcheck disable=SC2155
	declare -gr experiment_directory=$(pwd)

	# shellcheck disable=SC2155
	declare -gr binary_directory=$(cd ../../bin && pwd)

	# Globals used by parse_arguments
	declare -g target=""
	declare -g policy=""
	declare -g role=""

	# Configure environment variables
	export PATH=$binary_directory:$PATH
	export LD_LIBRARY_PATH=$binary_directory:$LD_LIBRARY_PATH
	export SLEDGE_NWORKERS=5
}

# $1 - Scheduler Variant (EDF|FIFO)
# $2 - Results Directory
# $3 - How to run (foreground|background)
# $4 - JSON specification
start_runtime() {
	printf "Starting Runtime: "
	if (($# != 4)); then
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
	elif ! [[ $3 =~ ^(foreground|background)$ ]]; then
		printf "[ERR]\n"
		error_msg "expected foreground or background was \"$3\""
		return 1
	elif [[ ! -f "$4" || "$4" != *.json ]]; then
		printf "[ERR]\n"
		error_msg "\"$4\" does not exist or is not a JSON"
		return 1
	fi

	local -r scheduler="$1"
	local -r results_directory="$2"
	local -r how_to_run="$3"
	local -r specification="$4"

	local -r log_name=log.txt
	local log="$results_directory/${log_name}"

	log_environment >> "$log"

	case "$how_to_run" in
		"background")
			SLEDGE_SCHEDULER="$scheduler" \
				sledgert "$specification" >> "$log" 2>> "$log" &
			;;
		"foreground")
			SLEDGE_SCHEDULER="$scheduler" \
				sledgert "$specification"
			;;
	esac

	printf "[OK]\n"
	return 0
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
		local -r results_directory="$experiment_directory/res/$timestamp/$scheduler"
		local -r how_to_run="background"
	elif [[ "$role" == "server" ]]; then
		local -r results_directory="$experiment_directory/res/$timestamp"
		local -r how_to_run="foreground"
	else
		error_msg "Unexpected $role"
		return 1
	fi

	mkdir -p "$results_directory"

	start_runtime "$scheduler" "$results_directory" "$how_to_run" "$experiment_directory/spec.json" || {
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

kill_runtime() {
	printf "Stopping Runtime: "
	pkill sledgert > /dev/null 2> /dev/null
	pkill hey > /dev/null 2> /dev/null
	printf "[OK]\n"
}

# Takes a variadic number of *.gnuplot filenames and generates the resulting images
# Assumes that the gnuplot definitions are in the experiment directory
generate_gnuplots() {
	if ! command -v gnuplot &> /dev/null; then
		echo "${FUNCNAME[0]} error: gnuplot could not be found in path"
		exit
	fi
	# shellcheck disable=SC2154
	if [ -z "$results_directory" ]; then
		echo "${FUNCNAME[0]} error: results_directory was unset or empty"
		dump_bash_stack
		exit 1
	fi
	# shellcheck disable=SC2154
	if [ -z "$experiment_directory" ]; then
		echo "${FUNCNAME[0]} error: experiment_directory was unset or empty"
		dump_bash_stack
		exit 1
	fi
	cd "$results_directory" || exit
	for gnuplot_file in "${@}"; do
		gnuplot "$experiment_directory/$gnuplot_file.gnuplot"
	done
	cd "$experiment_directory" || exit
}

# Takes a variadic number of paths to *.csv files and converts to *.dat files in the same directory
csv_to_dat() {
	if (($# == 0)); then
		echo "${FUNCNAME[0]} error: insufficient parameters"
		dump_bash_stack
	fi

	for arg in "$@"; do
		if ! [[ "$arg" =~ ".csv"$ ]]; then
			echo "${FUNCNAME[0]} error: $arg is not a *.csv file"
			dump_bash_stack
			exit 1
		fi
	done

	for file in "$@"; do
		echo -n "#" > "${file/.csv/.dat}"
		tr ',' ' ' < "$file" | column -t >> "${file/.csv/.dat}"
	done
}
