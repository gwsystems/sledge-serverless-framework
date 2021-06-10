# shellcheck shell=bash

# Example: Be sure to not set the -i attribute until after validating the content
# local -r second=${2:?second not set}
# check_number second || return 1
# local -i second
check_number() {
	local arg=${1:?arg not set}
	# echo "${arg_raw}: ${!arg_raw}"
	# A non-numeric string seems to coerce to 0
	((arg == 0)) && [[ ${!arg} != "0" ]] && echo "$arg contains ${!arg}, which is not a valid number" && return 1

	return 0
}

check_file() {
	local arg_raw=${1:?arg not set}
	local -n arg="$arg_raw"
	[[ ! -f "$arg" ]] && echo "${arg_raw} contains $arg, which is not a valid file" && return 1

	return 0
}

check_nameref() {
	# Namerefs automatically transitively resolve, so we have to use indirect expansion to get the name of the intermediate variable name
	local nameref_name=${1:?arg not set}
	local -n nameref="$nameref_name"
	local nameref_value=${!nameref}

	[[ ! -v nameref ]] && echo "nameref $nameref_name contains $nameref_value, which does not resolve to variable" && return 1

	return 0
}

check_argc() {
	local -i expected_argc="$1"
	local argv="$2"
	local -i actual_argc="${#argv}"
	((expected_argc != actual_argc)) && echo "expected ${expected_argc} received ${actual_argc}" && return 1

	return 0
}
