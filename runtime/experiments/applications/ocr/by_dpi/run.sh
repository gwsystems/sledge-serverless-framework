#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
# source generate_gnuplots.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1

# Validate that required tools are in path
declare -a required_binaries=(curl shuf pango-view pngtopnm diff)

validate_dependencies() {
	for required_binary in "${required_binaries[@]}"; do
		if ! command -v "$required_binary" > /dev/null; then
			echo "$required_binary is not present."
			exit 1
		fi
	done
}

experiment_main() {
	local -ri iteration_count=100
	local -ri word_count=100

	if (($# != 2)); then
		panic "invalid number of arguments \"$1\""
		return 1
	elif [[ -z "$1" ]]; then
		panic "hostname \"$1\" was empty"
		return 1
	elif [[ ! -d "$2" ]]; then
		panic "directory \"$2\" does not exist"
		return 1
	fi

	validate_dependencies

	local -r hostname="$1"
	local -r results_directory="$2"

	# Write Headers to CSV files
	printf "DPI,Success_Rate\n" >> "$results_directory/success.csv"
	printf "DPI,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	# Perform Experiments
	printf "Running Experiments\n"
	local -ar dpis=(72 108 144)
	local -Ar dpi_to_port=(
		[72]=10000
		[108]=10001
		[144]=10002
	)
	local words
	for ((i = 0; i < iteration_count; i++)); do
		words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

		for dpi in "${dpis[@]}"; do
			pango-view --dpi="$dpi" --font=mono -qo "${dpi}"_dpi.png -t "$words"
			pngtopnm "${dpi}"_dpi.png > "${dpi}"_dpi.pnm

			result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"${dpi}"_dpi.pnm "$hostname:${dpi_to_port[$dpi]}" --silent -w "%{stderr}%{time_total}\n" 2>> "$results_directory/${dpi}_time.txt")

			rm "${dpi}"_dpi.png "${dpi}"_dpi.pnm

			# Logs the number of words that don't match
			diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result") | wc -l >> "$results_directory/${dpi}_results.txt"
		done
	done

	# Process Results
	for dpi in "${dpis[@]}"; do
		# Skip empty results
		oks=$(wc -l < "$results_directory/${dpi}_time.txt")
		((oks == 0)) && continue

		# Calculate success rate
		awk '
			BEGIN {total_mistakes=0}
			{total_mistakes += $1}
			END {
				average_mistakes = (total_mistakes / NR)
				success_rate = ('$word_count' - average_mistakes) / '$word_count' * 100
				printf  "'"$dpi"',%f\n", success_rate
			}
		' < "$results_directory/${dpi}_results.txt" >> "$results_directory/success.csv"

		# Convert latency from s to ms, and sort
		awk -F, '{print ($0 * 1000)}' < "$results_directory/${dpi}_time.txt" | sort -g > "$results_directory/${dpi}_time_sorted.txt"

		# Generate Latency Data for csv
		awk '
			BEGIN {
				sum = 0
				p50 = int('"$oks"' * 0.5)
				p90 = int('"$oks"' * 0.9)
				p99 = int('"$oks"' * 0.99)
				p100 = '"$oks"'
				printf "'"$dpi"',"
			}
			NR==p50  {printf "%1.4f,",  $0}
			NR==p90  {printf "%1.4f,",  $0}
			NR==p99  {printf "%1.4f,",  $0}
			NR==p100 {printf "%1.4f\n", $0}
		' < "$results_directory/${dpi}_time_sorted.txt" >> "$results_directory/latency.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/latency.csv"
}

main "$@"
