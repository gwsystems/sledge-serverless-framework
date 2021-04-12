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

word_count=100
fonts=("DejaVu Sans Mono" "Roboto" "Cascadia Code")
total_count=10

for ((i = 1; i <= total_count; i++)); do
	echo "Test $i"
	words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

	for font in "${fonts[@]}"; do
		# For whatever reason, templating in multiple word strips was a pain, so brute forcing
		case "$font" in
			"DejaVu Sans Mono")
				echo "DejaVu Sans Mono"
				pango-view --font="DejaVu Sans Mono" -qo mono_words.png -t "$words" || exit 1
				pngtopnm mono_words.png > mono_words.pnm || exit 1
				result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @mono_words.pnm localhost:10000 2> /dev/null)
				diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result")
				;;
			"Roboto")
				echo "Roboto"
				pango-view --font="Roboto" -qo Roboto_words.png -t "$words" || exit 1
				pngtopnm Roboto_words.png > Roboto_words.pnm || exit 1
				result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @Roboto_words.pnm localhost:10002 2> /dev/null)
				diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result")
				;;
			"Cascadia Code")
				echo "Cascadia Code"
				pango-view --font="Cascadia Code" -qo Cascadia_Code_words.png -t "$words" || exit 1
				pngtopnm Cascadia_Code_words.png > Cascadia_Code_words.pnm || exit 1
				result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @Cascadia_Code_words.pnm localhost:10001 2> /dev/null)
				diff -ywBZE --suppress-common-lines <(echo "$words") <(echo "$result")
				;;
		esac
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
