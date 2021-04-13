#!/bin/bash

dump_bash_stack() {
	echo "Call Stack:"
	for func in "${FUNCNAME[@]}"; do
		echo "$func"
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
# $2 an optional return nameref. If not set, writes results to STDOUT
get_result_count() {
	if (($# != 1)); then
		echo "${FUNCNAME[0]} error: insufficient parameters"
		dump_bash_stack
	elif [[ ! -f $1 ]]; then
		echo "${FUNCNAME[0]} error: the file $1 does not exist"
		dump_bash_stack
	fi

	local -r file=$1

	# Subtract one line for the header
	local -i count=$(($(wc -l < "$file") - 1))

	if (($# == 2)); then
		local -n __result=$2
		__result=count
	else
		echo "$count"
	fi

	if ((count > 0)); then
		return 0
	else
		return 1
	fi
}

kill_runtime() {
	echo -n "Killing Runtime: "
	pkill sledgert > /dev/null 2> /dev/null
	pkill hey > /dev/null 2> /dev/null
	echo "[DONE]"
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
