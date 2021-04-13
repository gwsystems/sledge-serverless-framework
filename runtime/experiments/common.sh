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

kill_runtime() {
	printf "Stopping Runtime: "
	pkill sledgert > /dev/null 2> /dev/null
	pkill hey > /dev/null 2> /dev/null
	printf "[OK]\n"
}

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
	gnuplot ../../latency.gnuplot
	gnuplot ../../success.gnuplot
	gnuplot ../../throughput.gnuplot
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
