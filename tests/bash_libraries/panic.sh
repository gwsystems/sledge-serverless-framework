# shellcheck shell=bash
if [ -n "$__panic_sh__" ]; then return; fi
__panic_sh__=$(date)

source "error_msg.sh" || exit 1

declare __common_did_dump_callstack=false

__common_dump_callstack() {
	echo "Call Stack:"
	for ((i = 1; i < ${#FUNCNAME[@]}; i++)); do
		printf "\t%d - %s\n" "$((i - 1))" "${FUNCNAME[i]} (${BASH_SOURCE[i + 1]}:${BASH_LINENO[i]})"
	done
}

# Public API
panic() {
	error_msg "${@}"
	[[ "$__common_did_dump_callstack" == false ]] && {
		__common_dump_callstack
		__common_did_dump_callstack=true
	}
}
