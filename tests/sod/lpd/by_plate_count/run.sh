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
source percentiles_table.sh || exit 1
source validate_dependencies.sh || exit 1

get_random_image() {
	local workload="$1"
	local -n __random_image="$2"

	case $workload in
		lpd1) __random_image="${lpd1_images[$((RANDOM % ${#lpd1_images[@]}))]}" ;;
		lpd2) __random_image="${lpd2_images[$((RANDOM % ${#lpd2_images[@]}))]}" ;;
		lpd4) __random_image="${lpd4_images[$((RANDOM % ${#lpd4_images[@]}))]}" ;;
		*) panic "Invalid Workload" ;;
	esac
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

	printf "Processing Results: "

	# Write headers to CSVs
	percentiles_table_header "$results_directory/latency.csv"

	for workload in "${workloads[@]}"; do

		# Filter on 200s, subtract DNS time, convert from s to ms, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		# Generate Latency Data for csv
		percentiles_table_row "$results_directory/$workload-response.csv" "$results_directory/latency.csv" "$workload"

		# Delete scratch file used for sorting/counting
		rm -rf "$results_directory/$workload-response.csv"
	done

	# Transform csvs to dat files for gnuplot
	csv_to_dat "$results_directory/latency.csv"

	printf "[OK]\n"
	return 0
}

run_functional_tests() {
	local hostname="$1"
	local results_directory="$2"

	# Functional Testing on each image
	for image in "${lpd1_images[@]}"; do
		echo "@${image}" >> "${results_directory}/lpd1.txt"
		curl --data-binary "@${image}" --output - "${hostname}:10000/lpd1" >> "${results_directory}/lpd1.txt"
	done
	for image in "${lpd2_images[@]}"; do
		echo "@${image}" >> "${results_directory}/lpd2.txt"
		curl --data-binary "@${image}" --output - "${hostname}:10000/lpd2" >> "${results_directory}/lpd2.txt"
	done
	for image in "${lpd4_images[@]}"; do
		echo "@${image}" >> "${results_directory}/lpd4.txt"
		curl --data-binary "@${image}" --output - "${hostname}:10000/lpd4" >> "${results_directory}/lpd4.txt"
	done
}

run_perf_tests() {
	local hostname="$1"
	local results_directory="$2"

	local -ir total_iterations=100
	local -ir worker_max=10
	local -ir batch_size=10
	local -i batch_id=0
	local random_image
	local pids

	printf "Perf Tests: \n"
	for workload in "${workloads[@]}"; do
		batch_id=0
		for ((i = 0; i < total_iterations; i += batch_size)); do
			# Block waiting for a worker to finish if we are at our max
			while (($(pgrep --count hey) >= worker_max)); do
				wait -n $(pgrep hey | tr '\n' ' ')
			done
			((batch_id++))

			get_random_image "$workload" random_image
			hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m GET -D "${random_image}" "http://${hostname}:10000${path[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" 2> /dev/null &
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

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	run_functional_tests "$hostname" "$results_directory" || return 1
	run_perf_tests "$hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1
}

validate_dependencies curl

declare -a workloads=(lpd1 lpd2 lpd4)

declare -Ar path=(
	[lpd1]=/lpd1
	[lpd2]=/lpd2
	[lpd4]=/lpd4
)

# Sort the images by the number of labeled plates
declare -a lpd1_images=()
declare -a lpd2_images=()
declare -a lpd4_images=()
while IFS= read -r image_data; do
	image_file="${image_data/csv/png}"
	# Each line of csv data represents a labeled plate on the image
	case $(wc "$image_data" -l | cut -d\  -f1) in
		1) lpd1_images+=("$image_file") ;;
		2) lpd2_images+=("$image_file") ;;
		4) lpd4_images+=("$image_file") ;;
		*) panic "Unexpected number of plates" ;;
	esac
done < <(ls ../../../../applications/wasm_apps/sod/samples/plates/*.csv)

framework_init "$@"
