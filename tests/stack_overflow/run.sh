#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source framework.sh || exit 1
source validate_dependencies.sh || exit 1

experiment_client() {
	local -r hostname="$1"

	for ((i = 1; i <= 10; i++)); do
		http -p h "${hostname}:10000/stack_overflow" | grep 500 || {
			echo "FAIL"
			return 1
		}
	done

	echo "SUCCESS"
	return 0

}

validate_dependencies http

framework_init "$@"
