# shellcheck shell=bash
if [ -n "$__framework_sh__" ]; then return; fi
__framework_sh__=$(date)

#
# This framework simplifies the scripting of experiments
#
# To use, import the framework source file and pass all arguments to the provided main function
# source "framework.sh"
#
# main "$@"
#
# In your script, implement the following functions above main:
# - experiment_main
#

source "path_join.sh" || exit 1
source "panic.sh" || exit 1

__framework_sh__usage() {
	echo "$0 [options...]"
	echo ""
	echo "Options:"
	echo "  -t,--target=<target url> Execute as client against remote URL"
	echo "  -s,--serve=<EDF|FIFO>    Serve with scheduling policy, but do not run client"
	echo "  -d,--debug=<EDF|FIFO>    Debug under GDB with scheduling policy, but do not run client"
	echo "  -p,--perf=<EDF|FIFO>     Run under perf with scheduling policy. Run on baremetal Linux host!"
}

# Declares application level global state
__framework_sh__initialize_globals() {
	# timestamp is used to name the results directory for a particular test run
	# shellcheck disable=SC2155
	# shellcheck disable=SC2034
	declare -gir __framework_sh__timestamp=$(date +%s)

	# Globals used by parse_arguments
	declare -g __framework_sh__target=""
	declare -g __framework_sh__policy=""
	declare -g __framework_sh__role=""

	# Configure environment variables
	# shellcheck disable=SC2155
	declare -gr __framework_sh__application_directory="$(dirname "$(realpath "$0"))")"
	local -r binary_directory="$(cd "$__framework_sh__application_directory" && cd ../../bin && pwd)"
	export PATH=$binary_directory:$PATH
	export LD_LIBRARY_PATH=$binary_directory:$LD_LIBRARY_PATH
	export SLEDGE_NWORKERS=5
}

# Parses arguments from the user and sets associates global state
__framework_sh__parse_arguments() {
	for i in "$@"; do
		case $i in
			-t=* | --target=*)
				if [[ "$__framework_sh__role" == "server" ]]; then
					echo "Cannot set target when server"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=client
				__framework_sh__target="${i#*=}"
				shift
				;;
			-s=* | --serve=*)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -s,--serve with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=server
				__framework_sh__policy="${i#*=}"
				if [[ ! $__framework_sh__policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$__framework_sh__policy\" is not a valid policy. EDF or FIFO allowed"
					__framework_sh__usage
					return 1
				fi
				shift
				;;
			-d=* | --debug=*)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -d,--debug with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=debug
				__framework_sh__policy="${i#*=}"
				if [[ ! $__framework_sh__policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$__framework_sh__policy\" is not a valid policy. EDF or FIFO allowed"
					__framework_sh__usage
					return 1
				fi
				shift
				;;
			-p=* | --perf=*)
				if [[ "$__framework_sh__role" == "perf" ]]; then
					echo "Cannot use -p,--perf with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=perf
				__framework_sh__policy="${i#*=}"
				if [[ ! $__framework_sh__policy =~ ^(EDF|FIFO)$ ]]; then
					echo "\"$__framework_sh__policy\" is not a valid policy. EDF or FIFO allowed"
					__framework_sh__usage
					return 1
				fi
				shift
				;;
			-h | --help)
				__framework_sh__usage
				exit 0
				;;
			*)
				echo "$1 is a not a valid option"
				__framework_sh__usage
				return 1
				;;
		esac
	done

	# default to both if no arguments were passed
	if [[ -z "$__framework_sh__role" ]]; then
		__framework_sh__role="both"
		__framework_sh__target="localhost"
	fi

	# Set globals as read only
	declare -r __framework_sh__target
	declare -r __framework_sh__policy
	declare -r __framework_sh__role
}

# Log hardware and software info for the execution
__framework_sh__log_environment() {
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
	cat "$(path_join "$__framework_sh__application_directory" ../../Makefile)"
	echo ""

	echo "**********"
	echo "* Run.sh *"
	echo "**********"
	cat "$(path_join "$__framework_sh__application_directory" ./run.sh)"
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

# $1 - Scheduler Variant (EDF|FIFO)
# $2 - Results Directory
# $3 - How to run (foreground|background)
# $4 - JSON specification
__framework_sh__start_runtime() {
	printf "Starting Runtime: "
	if (($# != 4)); then
		printf "[ERR]\n"
		panic "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		panic "expected EDF or FIFO was \"$1\""
		return 1
	elif ! [[ -d "$2" ]]; then
		printf "[ERR]\n"
		panic "directory \"$2\" does not exist"
		return 1
	elif ! [[ $3 =~ ^(foreground|background)$ ]]; then
		printf "[ERR]\n"
		panic "expected foreground or background was \"$3\""
		return 1
	elif [[ ! -f "$4" || "$4" != *.json ]]; then
		printf "[ERR]\n"
		panic "\"$4\" does not exist or is not a JSON"
		return 1
	fi

	local -r scheduler="$1"
	local -r results_directory="$2"
	local -r how_to_run="$3"
	local -r specification="$4"

	local -r log_name=log.txt
	local log="$results_directory/${log_name}"

	__framework_sh__log_environment >> "$log"

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

__framework_sh__run_server() {
	if (($# != 2)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		panic "expected EDF or FIFO was \"$1\""
		return 1
	elif ! [[ $2 =~ ^(foreground|background)$ ]]; then
		printf "[ERR]\n"
		panic "expected foreground or background was \"$3\""
		return 1
	fi

	local -r scheduler="$1"
	local -r how_to_run="$2"

	__framework_sh__start_runtime "$scheduler" "$RESULTS_DIRECTORY" "$how_to_run" "$__framework_sh__application_directory/spec.json" || {
		echo "__framework_sh__start_runtime RC: $?"
		panic "Error calling __framework_sh__start_runtime $scheduler $RESULTS_DIRECTORY $how_to_run $__framework_sh__application_directory/spec.json"
		return 1
	}

	return 0
}

__framework_sh__run_perf() {
	if (($# != 1)); then
		printf "[ERR]\n"
		panic "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		panic "expected EDF or FIFO was \"$1\""
		return 1
	fi

	if ! command -v perf; then
		echo "perf is not present."
		exit 1
	fi

	local -r scheduler="$1"

	SLEDGE_SCHEDULER="$scheduler" perf record -g -s sledgert "$__framework_sh__application_directory/spec.json"
}

# Starts the Sledge Runtime under GDB
__framework_sh__run_debug() {
	# shellcheck disable=SC2155
	local project_directory=$(cd ../.. && pwd)
	if (($# != 1)); then
		printf "[ERR]\n"
		panic "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(EDF|FIFO)$ ]]; then
		printf "[ERR]\n"
		panic "expected EDF or FIFO was \"$1\""
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
			--eval-command="run $__framework_sh__application_directory/spec.json" \
			sledgert
	else
		SLEDGE_SCHEDULER="$scheduler" gdb \
			--eval-command="handle SIGUSR1 nostop" \
			--eval-command="handle SIGPIPE nostop" \
			--eval-command="set pagination off" \
			--eval-command="run $__framework_sh__application_directory/spec.json" \
			sledgert
	fi
	return 0
}

__framework_sh__run_client() {
	experiment_main "$__framework_sh__target" "$RESULTS_DIRECTORY" || {
		panic "Error calling process_results $RESULTS_DIRECTORY"
		return 1
	}

	return 0
}

__framework_sh__run_both() {
	local -ar schedulers=(EDF FIFO)
	for scheduler in "${schedulers[@]}"; do
		printf "Running %s\n" "$scheduler"

		__framework_sh__run_server "$scheduler" background || {
			panic "Error calling __framework_sh__run_server"
			return 1
		}

		__framework_sh__run_client || {
			panic "Error calling __framework_sh__run_client"
			__framework_sh__stop_runtime
			return 1
		}

		__framework_sh__stop_runtime || {
			panic "Error calling __framework_sh__stop_runtime"
			return 1
		}

	done

	return 0
}

__framework_sh__create_and_export_results_directory() {
	local dir=""

	# If we are running both client, and server, we need to namespace by scheduler since we run both variants
	case "$__framework_sh__role" in
		"both")
			dir="$__framework_sh__application_directory/res/$__framework_sh__timestamp/$scheduler"
			;;
		"client" | "server" | "debug" | "perf")
			dir="$__framework_sh__application_directory/res/$__framework_sh__timestamp"
			;;
		*)
			panic "${FUNCNAME[0]} Unexpected $__framework_sh__role"
			return 1
			;;
	esac

	mkdir -p "$dir" || {
		panic "mkdir -p $dir"
		return 1
	}

	export RESULTS_DIRECTORY="$dir"
}

# Responsible for ensuring that the experiment file meets framework assumptions
__framework_sh__validate_experiment() {
	if [[ $(type -t experiment_main) != "function" ]]; then
		panic "function experiment_main was not defined"
		return 1
	fi

}

main() {
	__framework_sh__validate_experiment || exit 1
	__framework_sh__initialize_globals || exit 1
	__framework_sh__parse_arguments "$@" || exit 1
	__framework_sh__create_and_export_results_directory || exit 1

	case $__framework_sh__role in
		both)
			__framework_sh__run_both
			;;
		server)
			__framework_sh__run_server "$__framework_sh__policy" foreground
			;;
		debug)
			__framework_sh__run_debug "$__framework_sh__policy"
			;;
		perf)
			__framework_sh__run_perf "$__framework_sh__policy"
			;;
		client)
			__framework_sh__run_client
			;;
		*)
			echo "Invalid state"
			false
			;;
	esac

	exit "$?"
}

__framework_sh__stop_runtime() {
	printf "Stopping Runtime: "
	pkill sledgert > /dev/null 2> /dev/null
	pkill hey > /dev/null 2> /dev/null
	printf "[OK]\n"
}
