#!/bin/bash

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1

run_functional_tests() {
	local -r hostname="$1"
	local -r results_directory="$2"
	local -i success_count=0
	local -ir total_count=50

	local expected_result
	expected_result="$(tr -d '\0' < ./expected_result.dat)"

	for ((i = 0; i < total_count; i++)); do
		result="$(curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@ekf_raw.dat" "$hostname:10000" 2> /dev/null | tr -d '\0')"
		if [[ "$expected_result" == "$result" ]]; then
			((success_count++))
		else
			{
				echo "Failed on $i:"
				echo "$result"
			} | tee -a "$results_directory/result.txt"
			break
		fi
	done

	echo "$success_count / $total_count" | tee -a "$results_directory/result.txt"

	if ((success_count == total_count)); then
		return 0
	else
		return 1
	fi
}

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	run_functional_tests "$hostname" "$results_directory" || return 1
}

# Copy data if not here
if  [[ ! -f "$__run_sh__base_path/initial_state.dat" ]]; then
	pushd "$__run_sh__base_path" || exit 1
	pushd "../../../../tests/TinyEKF/extras/c/" || exit 1
	cp ekf_raw.dat "$__run_sh__base_path/initial_state.dat" || exit 1
	popd || exit 1
	popd || exit 1
fi

framework_init "$@"
