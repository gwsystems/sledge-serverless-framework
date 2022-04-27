# shellcheck shell=bash
if [ -n "$__fn_exists_sh__" ]; then return; fi
__fn_exists_sh__=$(date)

fn_exists() {
	declare -f -F "$1" > /dev/null
	return $?
}
