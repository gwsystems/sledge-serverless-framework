#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source validate_dependencies.sh || exit 1

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	local -r expected_result="$(cat ./expected_result.txt)"
	local -i success_count=0
	local -ir total_count=10

	for ((i = 0; i < total_count; i++)); do
		result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@hyde.pnm" "$hostname:10000" 2> /dev/null)
		if [[ "$result" == "$expected_result" ]]; then
			((success_count++))
		else
			{
				echo "FAIL"
				echo "Expected:"
				echo "$expected_result"
				echo "==============================================="
				echo "Was:"
				echo "$result"
			} >> "$results_directory/results.txt"
			break
		fi
	done

	echo "$success_count / $total_count" >> "$results_directory/results.txt"

	if ((success_count == total_count)); then
		return 0
	else
		return 1
	fi
}

validate_dependencies curl

framework_init "$@"
