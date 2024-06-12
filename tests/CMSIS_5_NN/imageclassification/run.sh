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

get_random_image() {
	local -n __random_image="$1"

	__random_image="${cifar10_images[$((RANDOM % ${#cifar10_images[@]}))]}"
}

run_functional_tests() {
	local hostname="$1"
	local results_directory="$2"

	printf "Func Tests: \n"

	# Functional Testing on each image
	for image in "${cifar10_images[@]}"; do
		echo "${image}" >> "${results_directory}/cifar10_rand.txt"
		curl --data-binary "@${image}" -s "${hostname}:10000/rand" >> "${results_directory}/cifar10_rand.txt"
	done

	echo "$same_image" >> "${results_directory}/cifar10_same.txt"
	curl --data-binary "@$same_image" -s "${hostname}:10000/same" >> "${results_directory}/cifar10_same.txt"

	printf "[OK]\n"
}

run_perf_tests() {
	local hostname="$1"
	local results_directory="$2"

	local -ir total_iterations=100
	local -ir worker_max=10
	local -ir batch_size=10
	local -i batch_id=0
	local image
	local pids

	printf "Perf Tests: \n"
	for workload in "${workloads[@]}"; do
		batch_id=0
		for ((i = 0; i < total_iterations; i += batch_size)); do
			# Block waiting for a worker to finish if we are at our max
			while (($(pgrep --count hey) >= worker_max)); do
				wait -n "$(pgrep hey | tr '\n' ' ')"
			done
			((batch_id++))

			if [ "$workload" = "cifar10_rand" ]; then
				get_random_image image
			else
				image=$same_image
			fi
			hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m POST -D "${image}" "http://${hostname}:10000${route[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" 2> /dev/null &
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

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_results() {
	if (($# != 1)); then
		error_msg "invalid number of arguments ($#, expected 1)"
		return 1
	elif ! [[ -d "$1" ]]; then
		error_msg "directory $1 does not exist"
		return 1
	fi

	local -r results_directory="$1"

	printf "Processing Results: \n"

	# Write headers to CSVs
	percentiles_table_header "$results_directory/latency.csv"

	for workload in "${workloads[@]}"; do

		# Filter on 200s, subtract DNS time, convert from s to ms, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency.csv" "$workload"

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
	local same_image # for the cifar10_same workload

	get_random_image same_image

	run_functional_tests "$hostname" "$results_directory" || return 1
	run_perf_tests "$hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1
}

validate_dependencies curl

declare -ar workloads=(cifar10_rand cifar10_same)
declare -Ar route=(
	[cifar10_rand]=/rand
	[cifar10_same]=/same
)

# Sort the images by the number of labeled plates
declare -a cifar10_images=(../../../applications/wasm_apps/CMSIS_5_NN/images/bmp/*)

framework_init "$@"
