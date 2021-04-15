# shellcheck shell=bash
if [ -n "$__csv_to_dat_sh__" ]; then return; fi
__csv_to_dat_sh__=$(date)

source "panic.sh" || exit 1

# Takes a variadic number of paths to *.csv files and converts to *.dat files in the same directory
csv_to_dat() {
	if (($# == 0)); then
		panic "insufficient parameters"
		return 1
	fi

	for arg in "$@"; do
		if ! [[ "$arg" =~ ".csv"$ ]]; then
			panic "$arg is not a *.csv file"
			return 1
		fi
		if [[ ! -f "$arg" ]]; then
			panic "$arg does not exit"
			return 1
		fi
	done

	for file in "$@"; do
		echo -n "#" > "${file/.csv/.dat}"
		tr ',' ' ' < "$file" | column -t >> "${file/.csv/.dat}"
	done
}
