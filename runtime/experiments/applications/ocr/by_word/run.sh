#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
echo "$experiment_directory"
project_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)
log="$experiment_directory/log.csv"

if [ "$1" != "-d" ]; then
	SLEDGE_SANDBOX_PERF_LOG=$log PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" > rt.log 2>&1 &
	sleep 2
else
	echo "Running under gdb"
fi

word_counts=(1 10 100)

declare -A word_count_to_port
word_count_to_port["1_words.pnm"]=10000
word_count_to_port["10_words.pnm"]=10001
word_count_to_port["100_words.pnm"]=10002

total_count=100

for ((i = 0; i < total_count; i++)); do
	echo "$i"

	for word_count in "${word_counts[@]}"; do
		echo "${word_count}"_words.pnm
		words="$(shuf -n"$word_count" /usr/share/dict/american-english)"
		pango-view --font=mono -qo "$word_count"_words.png -t "$words" || exit 1
		pngtopnm "$word_count"_words.png > "$word_count"_words.pnm || exit 1

		result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"${word_count}"_words.pnm localhost:${word_count_to_port["$word_count"_words.pnm]} 2> /dev/null)

		# If the OCR does not produce a guess, fail
		[[ -z "$result" ]] && exit 1

		diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result")
		echo "==============================================="
	done

done

if [ "$1" != "-d" ]; then
	sleep 2
	echo -n "Running Cleanup: "
	rm ./*.png ./*.pnm
	pkill --signal sigterm sledgert > /dev/null 2> /dev/null
	sleep 2
	pkill sledgert -9 > /dev/null 2> /dev/null
	echo "[DONE]"
fi
