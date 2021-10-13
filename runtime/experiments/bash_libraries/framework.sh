# shellcheck shell=bash
if [ -n "$__framework_sh__" ]; then return; fi
__framework_sh__=$(date)

#
# This framework simplifies the scripting of experiments.
#
# It is designed around the idea of static experiments composed of one or more variants expressed by
# environment variables written to .env files. The default behavior is localhost mode, which runs a background
# server daemon and then execute a client driver script. If multiple .env files are defined, the framework
# automatically sets environment variables, starts the runtime as a background process, executes the client driver,
# stops the runtime, and clears the environment variables. The framework allows you to run the same logic on separate
# client and server hosts. It also provides various options to run under perf, gdb, valgrind, etc.
#
# To keep experiments relatively uniform, I suggest adding a single run.sh file inside your experiment.
#
# Your run.sh file should be started with the following snippet, which sources the framework.sh file
# and delegates all external arguments to the framework via the framework_init function. The first few lines are
# used to temporary add the directory containing BASH library scripts to your PATH environment variable. You may
# need to modify __run_sh__bash_libraries_relative_path to adjust the relative path depending on the location
# of your experimental directory.
#
###############################################################################################################################
# #!/bin/bash
# __run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
# __run_sh__bash_libraries_relative_path="../bash_libraries"
# __run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
# export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"
#
# source framework.sh || exit 1
#
# framework_init "$@"
###############################################################################################################################
#
# Use chmod +x run.sh to make your script executable, and then run ./run.sh --help to test it.
# You should see help information if successful.
#
# At this point, your script can run the server with defaults usings the --debug, --perf, --serve, and --valgrind
#
# To run a client or the default localhost mode that runs a client and a server on the same machine, you have to
# implement a function called experiment_client in run.sh above your call to framework_init.
#
# This function receives two arguments:
# - a results directory where you should intermediate files and reports / charts
# - a target hostname where you should target requests
#
# This function is called once per experimental variant, and the results directory is adjusted accordingly to
# keep your data organized.
#
# If your server logs data that you need to process, you can execute post-processing logic by implementing
# experiment_server_post, which gets called once per variant after the server background task terminates
#
# If you want to do one time setup before executing any variants, perform this logic in run.sh before calling
# framework_init.
#
# In addition to framework.sh, the bash_libraries directory contains a number of utility functions that are useful
# for cleaning and refactoring your data and error handling. Feel free to contribute utility functions to this
# directory if you write bash functions that you believe are reusable!
#
# It is also a good idea to look at the other experiments to get ideas for your script.
#
# If you are using VSCode, you have a number of extensions that help with bash development. This includes:
# - Bash IDE - an IntelliSense langauge server for bash
# - shell-format - Auto-format on save for bash
# - ShellCheck - an excellent linter for shell scripts.
#
# If you are not using VSCode, you may need to manually run shfmt against your script to keep formatting consistent
#
# Happy Scripting!

source "fn_exists.sh" || exit 1
source "path_join.sh" || exit 1
source "panic.sh" || exit 1
source "type_checks.sh" || exit 1
source "validate_dependencies.sh" || exit 1

__framework_sh__usage() {
	echo "$0 [options...]"
	echo ""
	echo "Options:"
	echo "  -d,--debug               Debug under GDB but do not run client"
	echo "  -e,--envfile=<file name> Load an Env File. No path and pass filename with *.env extension"
	echo "  -h,--help                Display usage information"
	echo "  -n,--name=<experiment>   Provide a unique name for this experimental run. Defaults to timestamp"
	echo "  -p,--perf                Run under perf. Limited to running on a baremetal Linux host!"
	echo "  -s,--serve               Serve but do not run client"
	echo "  -t,--target=<target url> Execute as client against remote URL"
	echo "  -v,--valgrind            Debug under Valgrind but do not run client"
}

# Declares application level global state
__framework_sh__initialize_globals() {
	# timestamp is used to name the results directory for a particular test run
	# This can be manually overridden via the name argument
	# shellcheck disable=SC2155
	# shellcheck disable=SC2034
	declare -gir __framework_sh__timestamp=$(date +%s)
	declare -g __framework_sh__experiment_name="$__framework_sh__timestamp"

	# Globals used by parse_arguments
	declare -g __framework_sh__target=""
	declare -g __framework_sh__role=""
	declare -g __framework_sh__envfile=""

	# Configure environment variables
	# shellcheck disable=SC2155
	declare -gr __framework_sh__application_directory="$(dirname "$(realpath "$0"))")"
	# shellcheck disable=SC2155
	declare -gr __framework_sh__path=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
	local -r binary_directory="$(cd "$__framework_sh__path" && cd ../../bin && pwd)"
	export PATH=$binary_directory:$PATH
	export LD_LIBRARY_PATH=$binary_directory:$LD_LIBRARY_PATH
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
			-s | --serve)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -s,--serve with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=server
				shift
				;;
			-d | --debug)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -d,--debug with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=debug
				shift
				;;
			-p | --perf)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -p,--perf with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=perf
				shift
				;;
			-v | --valgrind)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Cannot use -v,--valgrind with -t,--target"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__role=valgrind
				shift
				;;
			-n=* | --name=*)
				echo "Set experiment name to ${i#*=}"
				__framework_sh__experiment_name="${i#*=}"
				shift
				;;
			-e=* | --envfile=*)
				if [[ "$__framework_sh__role" == "client" ]]; then
					echo "Expected to be used with run by the server"
					__framework_sh__usage
					return 1
				fi
				__framework_sh__envfile="${i#*=}"
				echo "Set envfile to $__framework_sh__envfile"
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

	if [[ -z "$__framework_sh__envfile" ]]; then
		if [[ -d "$__framework_sh__application_directory/res/$__framework_sh__experiment_name/" ]]; then
			echo "Experiment $__framework_sh__experiment_name already exists. Pick a unique name!"
			exit 1
		fi
	else
		if [[ ! -f "$__framework_sh__envfile" ]]; then
			echo "$__framework_sh__envfile not found!!!"
			exit 1
		fi
		short_name="$(basename "${__framework_sh__envfile/.env/}")"
		echo "$__framework_sh__application_directory/res/$__framework_sh__experiment_name/$short_name/"
		if [[ -d "$__framework_sh__application_directory/res/$__framework_sh__experiment_name/$short_name/" ]]; then
			echo "Variant $short_name was already run in experiment ${__framework_sh__experiment_name}."
			exit 1
		fi
	fi

	# default to both if no arguments were passed
	if [[ -z "$__framework_sh__role" ]]; then
		__framework_sh__role="both"
		__framework_sh__target="localhost"
	fi

	# Set globals as read only
	declare -r __framework_sh__target
	declare -r __framework_sh__role
}

# Log hardware and software info for the execution
__framework_sh__log_environment() {
	validate_dependencies git
	echo "*******"
	echo "* Git *"
	echo "*******"
	git log | head -n 1 | cut -d' ' -f2
	git status
	echo ""

	echo "************"
	echo "* Spec.json *"
	echo "************"
	cat "$(path_join "$__framework_sh__application_directory" ./spec.json)"
	echo ""

	echo "************"
	echo "* Makefile *"
	echo "************"
	cat "$(path_join "$__framework_sh__path" ../../Makefile)"
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

# $1 - Results Directory
# $2 - How to run (foreground|background)
# $3 - JSON specification
__framework_sh__start_runtime() {
	printf "Starting Runtime: "
	if (($# != 2)); then
		printf "[ERR]\n"
		panic "invalid number of arguments \"$1\""
		return 1
	elif ! [[ $1 =~ ^(foreground|background)$ ]]; then
		printf "[ERR]\n"
		panic "expected foreground or background was \"$1\""
		return 1
	elif [[ ! -f "$2" || "$2" != *.json ]]; then
		printf "[ERR]\n"
		panic "\"$2\" does not exist or is not a JSON"
		return 1
	fi

	local -r how_to_run="$1"
	local -r specification="$2"

	local -r log_name=log.txt
	local log="$RESULTS_DIRECTORY/${log_name}"

	__framework_sh__log_environment >> "$log"

	case "$how_to_run" in
		"background")
			sledgert "$specification" >> "$log" 2>> "$log" &
			;;
		"foreground")
			sledgert "$specification"
			fn_exists experiment_server_post && experiment_server_post "$RESULTS_DIRECTORY"
			;;
	esac

	# Pad with a sleep to allow runtime to initialize before startup tasks run
	# This should be improved adding some sort of ping/ping heartbeat to the runtime
	# so the script can spin until initializaiton is complete
	sleep 1

	printf "[OK]\n"
	return 0
}

__framework_sh__run_server() {
	if (($# != 1)); then
		printf "[ERR]\n"
		panic "Invalid number of arguments. Saw $#. Expected 1."
		return 1
	elif ! [[ $1 =~ ^(foreground|background)$ ]]; then
		printf "[ERR]\n"
		panic "expected foreground or background was \"$3\""
		return 1
	fi

	local -r how_to_run="$1"

	__framework_sh__start_runtime "$how_to_run" "$__framework_sh__application_directory/spec.json" || {
		echo "__framework_sh__start_runtime RC: $?"
		panic "Error calling __framework_sh__start_runtime $how_to_run $__framework_sh__application_directory/spec.json"
		return 1
	}

	return 0
}

__framework_sh__run_perf() {
	validate_dependencies perf
	perf record -g -s sledgert "$__framework_sh__application_directory/spec.json"
	fn_exists experiment_server_post && experiment_server_post "$RESULTS_DIRECTORY"
}

__framework_sh__run_valgrind() {
	validate_dependencies valgrind
	valgrind --leak-check=full sledgert "$__framework_sh__application_directory/spec.json"
	fn_exists experiment_server_post && experiment_server_post "$RESULTS_DIRECTORY"
}

# Starts the Sledge Runtime under GDB
__framework_sh__run_debug() {
	validate_dependencies gdb
	# shellcheck disable=SC2155
	local project_directory=$(cd ../.. && pwd)

	if [[ "$project_directory" != "/sledge/runtime" ]]; then
		printf "It appears that you are not running in the container. Substituting path to match host environment\n"
		gdb \
			--eval-command="handle SIGUSR1 noprint nostop" \
			--eval-command="handle SIGPIPE noprint nostop" \
			--eval-command="set pagination off" \
			--eval-command="set substitute-path /sledge/runtime $project_directory" \
			--eval-command="run $__framework_sh__application_directory/spec.json" \
			sledgert
	else
		gdb \
			--eval-command="handle SIGUSR1 noprint nostop" \
			--eval-command="handle SIGPIPE noprint nostop" \
			--eval-command="set pagination off" \
			--eval-command="run $__framework_sh__application_directory/spec.json" \
			sledgert
	fi

	fn_exists experiment_server_post && experiment_server_post "$RESULTS_DIRECTORY"
	return 0
}

__framework_sh__run_client() {
	experiment_client "$__framework_sh__target" "$RESULTS_DIRECTORY" || return 1

	return 0
}

__framework_sh__load_env_file() {
	local envfile="${1:?envfile not defined}"
	[[ ! -f "$envfile" ]] && echo "envfile not found" && return 1

	local short_name
	short_name="$(basename "${envfile/.env/}")"
	printf "Running %s\n" "$short_name"

	while read -r line; do
		echo export "${line?}"
		export "${line?}"
	done < "$envfile"

	__framework_sh__create_and_export_results_directory "$short_name"
}

__framework_sh__unset_env_file() {
	local envfile="${1:?envfile not defined}"
	[[ ! -f "$envfile" ]] && echo "envfile not found" && return 1

	while read -r line; do
		echo unset "${line//=*/}"
		unset "${line//=*/}"
	done < "$envfile"
}

__framework_sh__run_both_env() {
	local envfile="${1:?envfile not defined}"
	__framework_sh__load_env_file "$envfile"

	__framework_sh__run_server background || {
		panic "Error calling __framework_sh__run_server"
		return 1
	}

	__framework_sh__run_client || {
		__framework_sh__unset_env_file "$envfile"
		__framework_sh__stop_runtime
		return 1
	}

	__framework_sh__stop_runtime || {
		panic "Error calling __framework_sh__stop_runtime"
		__framework_sh__unset_env_file "$envfile"
		return 1
	}

	__framework_sh__unset_env_file "$envfile"
}

# If envfile explicitly passed, just run that. Otherwise, run all
__framework_sh__run_both() {
	shopt -s nullglob

	if [[ -n "$__framework_sh__envfile" ]]; then
		__framework_sh__run_both_env "$__framework_sh__envfile"
	else
		local -i envfiles_found=0
		for envfile in "$__framework_sh__application_directory"/*.env; do
			((envfiles_found++))
			__framework_sh__run_both_env "$envfile"
		done
		((envfiles_found == 0)) && {
			echo "No *.env files found. Nothing to run!"
			exit 1
		}
	fi

	return 0
}

# Optionally accepts a subdirectory
# This is intended to namespace distinct runtime configs under a single namespace
__framework_sh__create_and_export_results_directory() {
	local -r subdirectory=${1:-""}

	local dir="$__framework_sh__application_directory/res/$__framework_sh__experiment_name/$subdirectory"

	mkdir -p "$dir" || {
		panic "mkdir -p $dir"
		return 1
	}

	export RESULTS_DIRECTORY="$dir"
}

# Responsible for ensuring that the experiment file meets framework assumptions
__framework_sh__validate_client() {
	if [[ $(type -t experiment_client) != "function" ]]; then
		panic "function experiment_client was not defined"
		return 1
	fi
}

framework_init() {
	__framework_sh__initialize_globals || exit 1
	__framework_sh__parse_arguments "$@" || exit 1
	__framework_sh__create_and_export_results_directory || exit 1

	case $__framework_sh__role in
		both)
			__framework_sh__validate_client || exit 1
			__framework_sh__run_both
			;;
		server)
			[[ -n "$__framework_sh__envfile" ]] && __framework_sh__load_env_file "$__framework_sh__application_directory/$__framework_sh__envfile"
			__framework_sh__run_server foreground
			;;
		debug)
			[[ -n "$__framework_sh__envfile" ]] && __framework_sh__load_env_file "$__framework_sh__application_directory/$__framework_sh__envfile"
			__framework_sh__run_debug
			;;
		perf)
			[[ -n "$__framework_sh__envfile" ]] && __framework_sh__load_env_file "$__framework_sh__application_directory/$__framework_sh__envfile"
			__framework_sh__run_perf
			;;
		valgrind)
			[[ -n "$__framework_sh__envfile" ]] && __framework_sh__load_env_file "$__framework_sh__application_directory/$__framework_sh__envfile"
			__framework_sh__run_valgrind
			;;
		client)
			__framework_sh__validate_client || exit 1
			__framework_sh__run_client
			;;
		*)
			echo "Invalid state"
			false
			;;
	esac

	return "$?"
}

__framework_sh__stop_runtime() {
	printf "Stopping Runtime: "
	# Ignoring RC of 1, as it indicates no matching process
	pkill sledgert > /dev/null 2> /dev/null
	(($? > 1)) && {
		printf "[ERR]\npkill sledgrt: %d\n" $?
		exit 1
	}
	pkill hey > /dev/null 2> /dev/null
	(($? > 1)) && {
		printf "[ERR]\npkill hey: %d\n" $?
		exit 1
	}

	fn_exists experiment_server_post && experiment_server_post "$RESULTS_DIRECTORY"
	printf "[OK]\n"
}
