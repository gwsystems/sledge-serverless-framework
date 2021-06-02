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
source validate_dependencies.sh || exit 1

get_random_image() {
	# local workload="$1"
	local -n __random_image="$1"

	__random_image="${cifar10_images[$((RANDOM % ${#cifar10_images[@]}))]}"
}

run_functional_tests() {
	local hostname="$1"
	local results_directory="$2"
	# same_image="./images/bmp/airplane5.bmp"

	printf "Func Tests: \n"

	# Functional Testing on each image
	for image in "${cifar10_images[@]}"; do
		echo "${image}" >> "${results_directory}/cifar10_rand.txt"
		curl --data-binary "@${image}" -s "${hostname}:10000" >> "${results_directory}/cifar10_rand.txt"
	done
	
	echo "$same_image" >> "${results_directory}/cifar10_same.txt"
	curl --data-binary "@$same_image" -s "${hostname}:10001" >> "${results_directory}/cifar10_same.txt"

	# file_type=bmp
	# # file_type=png

	# for class in airplane automobile bird cat deer dog frog horse ship truck; do
	# 	for instance in 1 2 3 4 5 6 7 8 9 10; do
	# 		echo "Classifying $class$instance.$file_type" >> "${results_directory}/cifar10_rand.txt"
	# 		curl -H 'Expect:' -H "Content-Type: Image/$file_type" --data-binary "@images/$file_type/$class$instance.$file_type" localhost:10000 >> "${results_directory}/cifar10_rand.txt" 
	# 	done
	# 	{
	# 		echo "==== ERROR RATE: x/10 ===="  # the 'x' is to be coded
	# 		echo ""
	# 		echo ""
	# 	} >> "${results_directory}/cifar10_rand.txt"
	# done

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
			hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m GET -D "${image}" "http://${hostname}:${port[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" 2> /dev/null &
			# curl --data-binary "@$image" --output - "${hostname}:${port[$workload]}" >> "${results_directory}/${workload}-2.txt"
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
				p50 = int('"$oks"' * 0.5)
				p90 = int('"$oks"' * 0.9)
				p99 = int('"$oks"' * 0.99)
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

experiment_main() {
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
declare -Ar port=(
	[cifar10_rand]=10000
	[cifar10_same]=10001
)

# Sort the images by the number of labeled plates
declare -a cifar10_images=(./images/bmp/*)

main "$@"
