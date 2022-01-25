# shellcheck shell=bash

if [ -n "$__path_join_sh__" ]; then return; fi
__path_join_sh__=$(date)

path_join() {
	local base=$1
	local relative=$2

	relative_path="$(dirname "$relative")"
	file_name="$(basename "$relative")"
	absolute_path="$(cd "$base" && cd "$relative_path" && pwd)"
	echo "$absolute_path/$file_name"
}
