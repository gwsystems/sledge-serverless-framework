# shellcheck shell=bash
# shellcheck disable=SC2034
if [ -n "$__experiment_server_globals_sh__" ]; then return; fi
__experiment_server_globals_sh__=$(date)

# The global configs for the scripts
declare -gr SERVER_LOG_FILE="perf.log"
declare -gr SERVER_HTTP_LOG_FILE="http_perf.log"
declare -gr NWORKERS=$(($(nproc)-2))

# Sandbox Perf Log Globals:
declare  -ga SANDBOX_METRICS=(total queued uninitialized allocated initialized runnable interrupted preempted running_sys running_user asleep returned complete error)
declare  -gA SANDBOX_METRICS_FIELDS=(
	[total]=6
	[queued]=7
	[uninitialized]=8
	[allocated]=9
	[initialized]=10
	[runnable]=11
	[interrupted]=12
	[preempted]=13
	[running_sys]=14
	[running_user]=15
	[asleep]=16
	[returned]=17
	[complete]=18
	[error]=19
)
declare -gr SANDBOX_TENANT_NAME_FIELD=2
declare -gr SANDBOX_ROUTE_FIELD=3
declare -gr SANDBOX_CPU_FREQ_FIELD=20
declare -gr SANDBOX_RESPONSE_CODE_FIELD=21

# HTTP Session Perf Log Globals:
declare  -ga HTTP_METRICS=(http_receive http_sent http_total)
declare  -gA HTTP_METRICS_FIELDS=(
	[http_receive]=6
	[http_sent]=7
	[http_total]=8
)
declare -gr HTTP_TENANT_NAME_FIELD=1
declare -gr HTTP_ROUTE_FIELD=2
declare -gr HTTP_CPU_FREQ_FIELD=9

assert_run_experiments_args() {
	if (($# != 3)); then
		panic "invalid number of arguments \"$#\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	elif [[ ! -d "$2" ]]; then
		panic "directory \"$2\" does not exist"
		return 1
	elif [[ -z "$3" ]]; then
		panic "load gen \"$3\" was empty"
		return 1
	fi
}

assert_process_client_results_args() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi
}

assert_process_server_results_args() {
	if (($# != 1)); then
		panic "invalid number of arguments \"$#\""
		return 1
	elif [[ ! -d "$1" ]]; then
		panic "directory \"$1\" does not exist"
		return 1
	fi
}
