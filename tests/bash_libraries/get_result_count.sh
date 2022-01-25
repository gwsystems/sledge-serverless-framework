# shellcheck shell=bash
if [ -n "$__get_result_count_sh__" ]; then return; fi
__get_result_count_sh__=$(date)

source "panic.sh" || exit 1

# Given a file, returns the number of results
# This assumes a *.csv file with a header
# $1 the file we want to check for results
# $2 an optional return nameref
get_result_count() {
	if (($# != 1)); then
		panic "insufficient parameters. $#/1"
		return 1
	elif [[ ! -f $1 ]]; then
		panic "the file $1 does not exist"
		return 1
	elif [[ ! -s $1 ]]; then
		panic "the file $1 is size 0"
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
