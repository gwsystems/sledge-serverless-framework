#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source validate_dependencies.sh || exit 1

run_functional_tests() {
	local hostname="$1"
	local results_directory="$2"

	local -i success_count=0
	local -ir total_count=10

	echo -n "Functional Tests: "

	for ((i = 0; i < total_count; i++)); do
		ext="$RANDOM"

		# Small
		if curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_small.jpg" --output "/tmp/result_${ext}_small.png" "${hostname}:10000" 2> /dev/null 1> /dev/null; then
			pixel_differences="$(compare -identify -metric AE "/tmp/result_${ext}_small.png" expected_result_small.png null: 2>&1 > /dev/null)"
			rm -f "/tmp/result_${ext}_small.png"
			if [[ "$pixel_differences" != "0" ]]; then
				echo "Small FAIL" >> "$results_directory/result.txt"
				echo "$pixel_differences pixel differences detected" >> "$results_directory/result.txt"
				continue
			fi
		else
			echo "curl failed with ${?}. See man curl for meaning." >> "$results_directory/result.txt"
			continue
		fi

		# Medium
		if curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_medium.jpg" --output "/tmp/result_${ext}_medium.png" "${hostname}:10001" 2> /dev/null 1> /dev/null; then
			pixel_differences="$(compare -identify -metric AE "/tmp/result_${ext}_medium.png" expected_result_medium.png null: 2>&1 > /dev/null)"
			rm -f "/tmp/result_${ext}_medium.png"
			if [[ "$pixel_differences" != "0" ]]; then
				echo "Medium FAIL" >> "$results_directory/result.txt"
				echo "$pixel_differences pixel differences detected" >> "$results_directory/result.txt"
				continue
			fi
		else
			echo "curl failed with ${?}. See man curl for meaning." >> "$results_directory/result.txt"
			continue
		fi

		# Large
		if curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_large.jpg" --output "/tmp/result_${ext}_large.png" "${hostname}:10002" 2> /dev/null 1> /dev/null; then
			pixel_differences="$(compare -identify -metric AE "/tmp/result_${ext}_large.png" expected_result_large.png null: 2>&1 > /dev/null)"
			rm -f "/tmp/result_${ext}_large.png"
			if [[ "$pixel_differences" != "0" ]]; then
				echo "Large FAIL" >> "$results_directory/result.txt"
				echo "$pixel_differences pixel differences detected" >> "$results_directory/result.txt"
				continue
			fi
		else
			echo "curl failed with ${?}. See man curl for meaning." >> "$results_directory/result.txt"
			continue
		fi

		((success_count++))
	done

	echo "$success_count / $total_count" >> "$results_directory/result.txt"

	echo "[OK]"
}

run_perf_tests() {
	local hostname="$1"
	local results_directory="$2"

	local -ir total_iterations=100
	local -ir worker_max=10
	local -ir batch_size=10
	local -i batch_id=0
	local pids

	echo -n "Perf Tests: "
	for workload in "${workloads[@]}"; do
		batch_id=0
		for ((i = 0; i < total_iterations; i += batch_size)); do
			# Block waiting for a worker to finish if we are at our max
			while (($(pgrep --count hey) >= worker_max)); do
				wait -n $(pgrep hey | tr '\n' ' ')
			done
			((batch_id++))

			hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m GET -D "shrinking_man_${workload}.jpg" "http://${hostname}:${port[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" 2> /dev/null &
		done
		pids=$(pgrep hey | tr '\n' ' ')
		[[ -n $pids ]] && wait -f $pids
	done
	printf "[OK]\n"

	for workload in "${workloads[@]}"; do
		tail --quiet -n +2 "$results_directory/${workload}"_*.csv >> "$results_directory/${workload}.csv"
		rm "$results_directory/${workload}"_*.csv
	done
}

process_results() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	local -r results_directory="$1"

	printf "Processing Results: "

	# Write headers to CSVs
	printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

	for workload in "${workloads[@]}"; do

		# Filter on 200s, subtract DNS time, convert from s to ms, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		oks=$(wc -l < "$results_directory/$workload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

		# Generate Latency Data for csv
		awk '
			BEGIN {
				sum = 0
				p50 = int('"$oks"' * 0.5) + 1
				p90 = int('"$oks"' * 0.9) + 1
				p99 = int('"$oks"' * 0.99) + 1
				p100 = '"$oks"'
				printf "'"$workload"',"
			}
			NR==p50  {printf "%1.4f,",  $0}
			NR==p90  {printf "%1.4f,",  $0}
			NR==p99  {printf "%1.4f,",  $0}
			NR==p100 {printf "%1.4f\n", $0}
		' < "$results_directory/$workload-response.csv" >> "$results_directory/latency.csv"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$workload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	run_functional_tests "$hostname" "$results_directory" || return 1
	run_perf_tests "$hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1

}

validate_dependencies curl

declare -ar workloads=(small medium large)
declare -Ar port=(
	[small]=10000
	[medium]=10001
	[large]=10002
)

framework_init "$@"
