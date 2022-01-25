# shellcheck shell=bash
if [ -n "$__error_msg_sh__" ]; then return; fi
__error_msg_sh__=$(date)

error_msg() {
	printf "%.23s %s() at %s:%s - %s\n" "$(date +%F.%T.%N)" "${FUNCNAME[0]}" "$(realpath "${BASH_SOURCE[0]##*/}")" "${BASH_LINENO[0]}" "${@}"
}
