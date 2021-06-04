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
source validate_dependencies.sh || exit 1

experiment_client() {
	local -ir iteration_count=100
	local -ra word_counts=(1 10 100)

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

	# Write Headers to CSV files

	local -Ar word_count_to_port=(
		["1_words"]=10000
		["10_words"]=10001
		["100_words"]=10002
	)

	local words
	for ((i = 0; i < iteration_count; i++)); do
		for word_count in "${word_counts[@]}"; do
			words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

			word_count_file="${word_count}_words"

			pango-view --font=mono -qo "$word_count_file.png" -t "$words" || exit 1
			pngtopnm "$word_count_file.png" > "$word_count_file.pnm" || exit 1

			result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"$word_count_file.pnm" "$hostname:${word_count_to_port[$word_count_file]}" --silent -w "%{stderr}%{time_total}\n" 2>> "$results_directory/${word_count_file}_time.txt")

			# If the OCR does not produce a guess, fail
			[[ -z "$result" ]] && exit 1

			rm "$word_count_file.png" "$word_count_file.pnm"

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
	printf "words,Success_Rate\n" >> "$results_directory/success.csv"
	printf "words,min,mean,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"
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
		awk '
			BEGIN {
				sum = 0
				p50_idx = int('"$oks"' * 0.5)
				p90_idx = int('"$oks"' * 0.9)
				p99_idx = int('"$oks"' * 0.99)
				p100_idx = '"$oks"'
				printf "'"$word_count_file"',"
			}
			             {sum += $0}
			NR==1        {min  = $0}
			NR==p50_idx  {p50  = $0}
			NR==p90_idx  {p90  = $0}
			NR==p99_idx  {p99  = $0}
			NR==p100_idx {p100 = $0}
			END {
				mean = sum / NR
				printf "%1.4f,%1.4f,%1.4f,%1.4f,%1.4f,%1.4f\n", min, mean, p50, p90, p99, p100
			}
		' < "$results_directory/${word_count_file}_time_sorted.txt" >> "$results_directory/latency.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/success.csv" "$results_directory/latency.csv"
}

validate_dependencies curl shuf pango-view pngtopnm diff

framework_init "$@"
