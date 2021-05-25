# shellcheck shell=bash

if [ -n "$__validate_dependencies_sh__" ]; then return; fi
__validate_dependencies_sh__=$(date)

validate_dependencies() {
	for required_binary in "$@"; do
		if ! command -v "$required_binary" > /dev/null; then
			echo "$required_binary is not present."
			return 1
		fi
	done

	return 0
}
