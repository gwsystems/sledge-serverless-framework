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
	local -r hostname="$1"
	local -r results_directory="$2"

	local -ir iteration_count=100
	local -ra word_counts=(1 10 100)

	local -Ar word_count_to_path=(
		["1_words"]=/gocr_1_word
		["10_words"]=/gocr_10_words
		["100_words"]=/gocr_100_words
	)

	local words
	for ((i = 0; i < iteration_count; i++)); do
		for word_count in "${word_counts[@]}"; do
			words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

			word_count_file="${word_count}_words"

			pango-view --font=mono -qo "$word_count_file.png" -t "$words" || exit 1

			result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"$word_count_file.png" "$hostname:10000${word_count_to_path[$word_count_file]}" --silent -w "%{stderr}%{time_total}\n" 2>> "$results_directory/${word_count_file}_time.txt")

			# If the OCR does not produce a guess, fail
			[[ -z "$result" ]] && exit 1

			rm "$word_count_file.png"

			# Logs the number of words that don't match
			# Also tees the full diff into a separate file
			echo "word count: $word_count" >> "$results_directory/${word_count_file}_full_results.txt"
			diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result") \
				| tee -a "$results_directory/${word_count_file}_full_results.txt" \
				| wc -l >> "$results_directory/${word_count_file}_results.txt"
			echo "==========================================" >> "$results_directory/${word_count_file}_full_results.txt"
		done
	done

	# Process Results
	# Write Headers to CSV files
	printf "words,Success_Rate\n" >> "$results_directory/success.csv"
	percentiles_table_header "$results_directory/latency.csv" "words"

	for word_count in "${word_counts[@]}"; do
		word_count_file="${word_count}_words"

		# Skip empty results
		oks=$(wc -l < "$results_directory/${word_count_file}_time.txt")
		((oks == 0)) && continue

		# Calculate success rate
		awk '
			BEGIN {total_mistakes=0}
			{total_mistakes += $1}
			END {
				average_mistakes = (total_mistakes / NR)
				success_rate = ('"$word_count"' - average_mistakes) / '"$word_count"' * 100
				printf  "'"$word_count_file"',%f\n", success_rate
			}
		' < "$results_directory/${word_count_file}_results.txt" >> "$results_directory/success.csv"

		# Convert latency from s to ms, and sort
		awk -F, '{print ($0 * 1000)}' < "$results_directory/${word_count_file}_time.txt" | sort -g > "$results_directory/${word_count_file}_time_sorted.txt"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/${word_count_file}_time_sorted.txt" "$results_directory/latency.csv" "$word_count_file"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/latency.csv"
}

validate_dependencies curl shuf pango-view diff

framework_init "$@"
