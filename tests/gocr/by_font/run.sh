#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source percentiles_table.sh || exit 1
source validate_dependencies.sh || exit 1

experiment_client() {
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

	local -r hostname="$1"
	local -r results_directory="$2"

	# Perform Experiments
	printf "Running Experiments\n"
	local -ra fonts=("DejaVu Sans Mono" "Roboto" "Cascadia Code")
	local -Ar font_to_path=(
		["DejaVu Sans Mono"]=/gocr_mono
		["Roboto"]=/gocr_roboto
		["Cascadia Code"]=/gocr_cascadia
	)
	local words
	for ((i = 1; i <= iteration_count; i++)); do
		words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

		for font in "${fonts[@]}"; do
			font_file="${font// /_}"

			pango-view --font="$font" -qo "${font_file}_words.png" -t "$words" || exit 1
			pngtopnm "${font_file}_words.png" > "${font_file}_words.pnm"

			result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"${font_file}_words.pnm" "$hostname:10000${font_to_path[$font]}" --silent -w "%{stderr}%{time_total}\n" 2>> "$results_directory/${font_file}_time.txt")

			rm "${font_file}"_words.png "${font_file}"_words.pnm

			# Logs the number of words that don't match
			echo "font: $font_file" >> "$results_directory/${font_file}_full_results.txt"
			diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result") \
				| tee -a "$results_directory/${font_file}_full_results.txt" \
				| wc -l >> "$results_directory/${font_file}_results.txt"
			echo "==========================================" >> "$results_directory/${font_file}_full_results.txt"
		done
	done

	# Process Results
	# Write Headers to CSV files
	printf "font,Success_Rate\n" >> "$results_directory/success.csv"
	percentiles_table_header "$results_directory/latency.csv"
	for font in "${fonts[@]}"; do
		font_file="${font// /_}"

		# Skip empty results
		oks=$(wc -l < "$results_directory/${font_file}_time.txt")
		((oks == 0)) && continue

		# Calculate success rate
		awk '
			BEGIN {total_mistakes=0}
			{total_mistakes += $1}
			END {
				average_mistakes = (total_mistakes / NR)
				success_rate = ('$word_count' - average_mistakes) / '$word_count' * 100
				printf  "'"$font_file"',%f\n", success_rate
			}
		' < "$results_directory/${font_file}_results.txt" >> "$results_directory/success.csv"

		# Convert latency from s to ms, and sort
		awk -F, '{print ($0 * 1000)}' < "$results_directory/${font_file}_time.txt" | sort -g > "$results_directory/${font_file}_time_sorted.txt"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/${font_file}_time_sorted.txt" "$results_directory/latency.csv" "$font_file"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/latency.csv"
}

validate_dependencies curl shuf pango-view pngtopnm diff

framework_init "$@"
