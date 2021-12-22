#!/bin/bash

# Add bash_libraries directory to path
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

run_functional_tests() {
	local hostname="$1"
	local results_directory="$2"

	local -i success_count=0
	local -ir total_count=50

	local tmpfs_dir=/tmp/sledge_ekf_by_iteration
	rm -rf "$tmpfs_dir"
	mkdir "$tmpfs_dir" || {
		echo "Failed to create tmp directory"
		return 1
	}

	for ((i = 0; i < total_count; i++)); do
		curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@initial_state.dat" "$hostname":10000 2> /dev/null > "$tmpfs_dir/one_iteration_res.dat"
		curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@$tmpfs_dir/one_iteration_res.dat" "$hostname":10001 2> /dev/null > "$tmpfs_dir/two_iterations_res.dat"
		curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@$tmpfs_dir/two_iterations_res.dat" "$hostname":10002 2> /dev/null > "$tmpfs_dir/three_iterations_res.dat"

		if diff -s "$tmpfs_dir/one_iteration_res.dat" one_iteration.dat > /dev/null \
			&& diff -s "$tmpfs_dir/two_iterations_res.dat" two_iterations.dat > /dev/null \
			&& diff -s "$tmpfs_dir/three_iterations_res.dat" three_iterations.dat > /dev/null; then
			((success_count++))
		fi

	done

	rm -r "$tmpfs_dir"
	if ((success_count == total_count)); then
		return 0
	else
		return 1
	fi
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

			hey -disable-compression -disable-keepalive -disable-redirects -n $batch_size -c 1 -cpus 1 -t 0 -o csv -m GET -D "./${workload}.dat" "http://${hostname}:${port[$workload]}" > "$results_directory/${workload}_${batch_id}.csv" &
		done
		pids=$(pgrep hey | tr '\n' ' ')
		[[ -n $pids ]] && wait -f $pids
	done
	echo "[OK]"

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

	printf "Processing Results: "

	# Write headers to CSVs
	percentiles_table_header "$results_directory/latency.csv"

	for workload in "${workloads[@]}"; do

		# Filter on 200s, subtract DNS time, convert from s to ms, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000)}' < "$results_directory/$workload.csv" \
			| sort -g > "$results_directory/$workload-response.csv"

		oks=$(wc -l < "$results_directory/$workload-response.csv")
		((oks == 0)) && continue # If all errors, skip line

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

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	run_functional_tests "$hostname" "$results_directory" || return 1
	run_perf_tests "$hostname" "$results_directory" || return 1
	process_results "$results_directory" || return 1

	return 0
}

# Copy data if not here
if  [[ ! -f "$__run_sh__base_path/initial_state.dat" ]]; then
	pushd "$__run_sh__base_path" || exit 1
	pushd "../../../../tests/TinyEKF/extras/c/" || exit 1
	cp ekf_raw.dat "$__run_sh__base_path/initial_state.dat" || exit 1
	popd || exit 1
	popd || exit 1
fi

declare -a workloads=(initial_state one_iteration two_iterations)
declare -A port=(
	[initial_state]=10000
	[one_iteration]=10001
	[two_iterations]=10002
)

framework_init "$@"
