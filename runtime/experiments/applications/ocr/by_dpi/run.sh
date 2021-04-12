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
dpis=(72 108 144)

declare -A dpi_to_port
dpi_to_port[72]=10000
dpi_to_port[108]=10001
dpi_to_port[144]=10002

total_count=100

for ((i = 0; i < total_count; i++)); do
	echo "$i"
	words="$(shuf -n"$word_count" /usr/share/dict/american-english)"

	for dpi in "${dpis[@]}"; do
		echo "${dpi}"_dpi.pnm
		pango-view --dpi="$dpi" --font=mono -qo "${dpi}"_dpi.png -t "$words"
		pngtopnm "${dpi}"_dpi.png > "${dpi}"_dpi.pnm

		result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @"${dpi}"_dpi.pnm localhost:${dpi_to_port[$dpi]} 2> /dev/null)

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
