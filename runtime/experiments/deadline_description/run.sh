#!/bin/bash

set -x

# Add bash_libraries directory to path
__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1

declare -a workloads=(ekf resize lpd gocr)

profile() {
	local hostname="$1"
	local -r results_directory="$2"

	echo "$results_directory/ekf/benchmark.csv"

	# ekf
	mkdir "$results_directory/ekf"
	hey -disable-compression -disable-keepalive -disable-redirects -n 16 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./ekf/initial_state.dat" "http://${hostname}:10000" > /dev/null
	hey -disable-compression -disable-keepalive -disable-redirects -n 256 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./ekf/initial_state.dat" "http://${hostname}:10000" > "$results_directory/ekf/benchmark.csv"

	# Resize
	mkdir "$results_directory/resize"
	hey -disable-compression -disable-keepalive -disable-redirects -n 16 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./resize/shrinking_man_large.jpg" "http://${hostname}:10001" > /dev/null
	hey -disable-compression -disable-keepalive -disable-redirects -n 256 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./resize/shrinking_man_large.jpg" "http://${hostname}:10001" > "$results_directory/resize/benchmark.csv"

	# lpd
	mkdir "$results_directory/lpd"
	hey -disable-compression -disable-keepalive -disable-redirects -n 16 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./lpd/Cars0.png" "http://${hostname}:10002" > /dev/null
	hey -disable-compression -disable-keepalive -disable-redirects -n 256 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./lpd/Cars0.png" "http://${hostname}:10002" > "$results_directory/lpd/benchmark.csv"

	# gocr - Hit error. Commented out temporarily
	# mkdir "$results_directory/gocr"
	# hey -disable-compression -disable-keepalive -disable-redirects -n 16 -c 1 -cpus 1 -t 0 -o csv -H 'Expect:' -H "Content-Type: text/plain" -m GET -D "./gocr/hyde.pnm" "http://${hostname}:10003" > /dev/null
	# hey -disable-compression -disable-keepalive -disable-redirects -n 256 -c 1 -cpus 1 -t 0 -o csv -m GET -D "./gocr/hyde.pnm" "http://${hostname}:10003" > "$results_directory/gocr/benchmark.csv"
}

get_baseline_execution() {
	local -r results_directory="$1"
	local -r module="$2"
	local -ir percentile="$3"

	local response_times_file="$results_directory/$module/response_times_sorted.csv"

	# Skip empty results
	local -i oks
	oks=$(wc -l < "$response_times_file")
	((oks == 0)) && return 1

	# Generate Latency Data for csv
	awk '
		BEGIN {idx = int('"$oks"' * ('"$percentile"' / 100))}
		NR==idx  {printf "%1.4f\n", $0}
	' < "$response_times_file"
}

get_random_from_interval() {
	local -r lower="$1"
	local -r upper="$2"
	awk "BEGIN { \"date +%N\" | getline seed; srand(seed); print rand() * ($upper - $lower) + $lower}"
}

calculate_relative_deadline() {
	local -r baseline="$1"
	local -r multiplier="$2"
	awk "BEGIN { printf \"%.0f\n\", ($baseline * $multiplier)}"
}

generate_relative_deadline() {
	local -r results_directory="$1"
	local -r workload="$2"

	local baseline
	local multiplier
	local relative_deadline

	local -ri percentile=90

	baseline="$(get_baseline_execution "$results_directory" "$workload" $percentile)"
	multiplier="$(get_random_from_interval 1.5 2.0)"
	relative_deadline=$(calculate_relative_deadline "$baseline" "$multiplier")
	echo "$relative_deadline"
}

generate_spec() {
	local results_directory="$1"

	# Run initial test run to get CDF of execution time for each app
	# I have to do this out-of-band on the

	local ekf_relative_deadline="$(generate_relative_deadline "$results_directory" ekf)"
	local resize_relative_deadline="$(generate_relative_deadline "$results_directory" resize)"
	local lpd_relative_deadline="$(generate_relative_deadline "$results_directory" lpd)"

	# Our JSON format is not spec complaint. I have to hack in a wrapping array before jq and delete it afterwards
	# Excluding gocr because of difficulty used gocr with hey
	{
		echo "["
		cat ./spec.json
		echo "]"
	} | jq "\
	[ \
		.[] | \
		if (.name == \"ekf\") then . + { \"relative-deadline-us\": $ekf_relative_deadline} else . end | \
		if (.name == \"resize\") then . + { \"relative-deadline-us\": $resize_relative_deadline} else . end | \
		if (.name == \"lpd\") then . + { \"relative-deadline-us\": $lpd_relative_deadline} else . end \
	]" | tail -n +2 | head -n-1 > "$results_directory/spec.json"

	# Get the baseline execution using a target in this CDF
	# generate a relative deadline per module
	# Use JQ to template baseline as "expected-execution-us" and deadline at "relative-deadline-us"
	# Execute experiment with deadlines and workload mix (all equal for now)
	# Capture "sandbox state" log and perhaps other logs?

}

# Process the experimental results and generate human-friendly results for success rate, throughput, and latency
process_results() {
	local results_directory="$1"

	for workload in "${workloads[@]}"; do
		# Filter on 200s, subtract DNS time, convert from s to ns, and sort
		awk -F, '$7 == 200 {print (($1 - $2) * 1000000)}' < "$results_directory/$workload/benchmark.csv" \
			| sort -g > "$results_directory/$workload/response_times_sorted.csv"
	done

	generate_spec "$results_directory"

	return 0
}

experiment_main() {
	local -r hostname="$1"
	local -r results_directory="$2"

	profile "$hostname" "$results_directory" || return 1
	process_results "$results_directory"
}

main "$@"
