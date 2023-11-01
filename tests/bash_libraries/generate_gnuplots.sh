# shellcheck shell=bash
if [ -n "$__generate_gnuplots_sh__" ]; then return; fi
__generate_gnuplots_sh__=$(date)

source "panic.sh" || exit 1

# Runs all *.gnuplot files found gnuplot_directory from results_directory
# Outputting resulting diagrams in results_directory
# $1 - results_directory containing the data file referenced in the gnuplot file
# $2 - gnuplot_directory containing the *.gnuplot specification files
generate_gnuplots() {
	local -r results_directory="$1"
	local -r experiment_directory="$2"

	if ! command -v gnuplot &> /dev/null; then
		panic "gnuplot could not be found in path"
		return 1
	fi
	# shellcheck disable=SC2154
	if [ -z "$results_directory" ]; then
		panic "results_directory was unset or empty"
		return 1
	fi
	# shellcheck disable=SC2154
	if [ -z "$experiment_directory" ]; then
		panic "error: EXPERIMENT_DIRECTORY was unset or empty"
		return 1
	fi
	cd "$results_directory" || exit

	shopt -s nullglob
	for gnuplot_file in "$experiment_directory"/*.gnuplot; do
		if [ -z "$TENANT_IDS" ]; then
			gnuplot "$gnuplot_file"
		else 
			gnuplot -e "tenant_ids='${TENANT_IDS[*]}'" "$gnuplot_file"
		fi
	done
	cd "$experiment_directory" || exit
}
