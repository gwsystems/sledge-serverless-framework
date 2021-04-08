#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Use -d flag if running under gdb

timestamp=$(date +%s)
experiment_directory=$(pwd)
binary_directory=$(cd ../../bin && pwd)
results_directory="$experiment_directory/res/$timestamp"
log=log.txt

mkdir -p "$results_directory"

log_environment >> "$results_directory/$log"

# Start the runtime
if [ "$1" != "-d" ]; then
	PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" >> "$results_directory/$log" 2>> "$results_directory/$log" &
	sleep 1
else
	echo "Running under gdb"
	echo "Running under gdb" >> "$results_directory/$log"
fi
payloads=(fivebyeight/5x8 handwriting/handwrt1 hyde/hyde)
ports=(10000 10001 10002)
iterations=1000

# Execute workloads long enough for runtime to learn excepted execution time
echo -n "Running Samples: "
for i in {0..2}; do
	hey -n 200 -c 3 -q 200 -o csv -m GET -D "$experiment_directory/${payloads[$i]}.pnm" "http://localhost:${ports[$i]}"
done
sleep 1
echo "[DONE]"

# Execute the experiments
echo "Running Experiments"
for i in {0..2}; do
	printf "\t%s Payload: " "${payloads[$i]}"
	file=$(echo "${payloads[$i]}" | awk -F/ '{print $2}').csv
	hey -n "$iterations" -c 3 -cpus 2 -o csv -m GET -D "$experiment_directory/${payloads[$i]}.pnm" "http://localhost:${ports[$i]}" > "$results_directory/$file"
	echo "[DONE]"
done

# Stop the runtime

if [ "$1" != "-d" ]; then
	sleep 5
	kill_runtime
fi

# Generate *.csv and *.dat results
echo -n "Parsing Results: "

printf "Concurrency,Success_Rate\n" >> "$results_directory/success.csv"
printf "Concurrency,Throughput\n" >> "$results_directory/throughput.csv"
printf "Con,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

for payload in ${payloads[*]}; do
	# Calculate Success Rate for csv
	file=$(echo "$payload" | awk -F/ '{print $2}')
	awk -F, '
    $7 == 200 {ok++}
    END{printf "'"$file"',%3.5f\n", (ok / '"$iterations"' * 100)}
  ' < "$results_directory/$file.csv" >> "$results_directory/success.csv"

	# Filter on 200s, convery from s to ms, and sort
	awk -F, '$7 == 200 {print ($1 * 1000)}' < "$results_directory/$file.csv" \
		| sort -g > "$results_directory/$file-response.csv"

	# Get Number of 200s
	oks=$(wc -l < "$results_directory/$file-response.csv")
	((oks == 0)) && continue # If all errors, skip line

	# Get Latest Timestamp
	duration=$(tail -n1 "$results_directory/$file.csv" | cut -d, -f8)
	throughput=$(echo "$oks/$duration" | bc)
	printf "%s,%f\n" "$file" "$throughput" >> "$results_directory/throughput.csv"

	# Generate Latency Data for csv
	awk '
    BEGIN {
      sum = 0
      p50 = int('"$oks"' * 0.5)
      p90 = int('"$oks"' * 0.9)
      p99 = int('"$oks"' * 0.99)
      p100 = '"$oks"'
      printf "'"$file"',"
    }   
    NR==p50 {printf "%1.4f,", $0}
    NR==p90 {printf "%1.4f,", $0}
    NR==p99 {printf "%1.4f,", $0}
    NR==p100 {printf "%1.4f\n", $0}
  ' < "$results_directory/$file-response.csv" >> "$results_directory/latency.csv"

	# Delete scratch file used for sorting/counting
	rm -rf "$results_directory/$file-response.csv"
done

# Transform csvs to dat files for gnuplot
for file in success latency throughput; do
	echo -n "#" > "$results_directory/$file.dat"
	tr ',' ' ' < "$results_directory/$file.csv" | column -t >> "$results_directory/$file.dat"
done

# Generate gnuplots
generate_gnuplots

# Cleanup, if requires
echo "[DONE]"
