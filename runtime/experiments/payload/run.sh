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

payloads=(1024 10240 102400 1048576)
ports=(10000 10001 10002 10003)
iterations=10000

# If the one of the expected body files doesn't exist, trigger the generation script.
for payload in ${payloads[*]}; do
	if test -f "$experiment_directory/body/$payload.txt"; then
		continue
	else
		echo "Generating Payloads: "
		{
			cd  "$experiment_directory/body" && ./generate.sh
		}
		break
	fi
done

# Execute workloads long enough for runtime to learn excepted execution time
echo -n "Running Samples: "
hey -n "$iterations" -c 3 -q 200 -o csv -m GET -D "$experiment_directory/body/1024.txt" http://localhost:10000
hey -n "$iterations" -c 3 -q 200 -o csv -m GET -D "$experiment_directory/body/10240.txt" http://localhost:10001
hey -n "$iterations" -c 3 -q 200 -o csv -m GET -D "$experiment_directory/body/102400.txt" http://localhost:10002
hey -n "$iterations" -c 3 -q 200 -o csv -m GET -D "$experiment_directory/body/1048576.txt" http://localhost:10003
sleep 5
echo "[DONE]"

# Execute the experiments
echo "Running Experiments"
for i in {0..3}; do
	printf "\t%d Payload: " "${payloads[$i]}"
	hey -n "$iterations" -c 1 -cpus 2 -o csv -m GET -D "$experiment_directory/body/${payloads[$i]}.txt" http://localhost:"${ports[$i]}" > "$results_directory/${payloads[$i]}.csv"
	echo "[DONE]"
done

# Stop the runtime
if [ "$1" != "-d" ]; then
	sleep 5
	kill_runtime
fi

# Generate *.csv and *.dat results
echo -n "Parsing Results: "

printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

for payload in ${payloads[*]}; do
	# Calculate Success Rate for csv
	awk -F, '
    $7 == 200 {ok++}
    END{printf "'"$payload"',%3.5f\n", (ok / '"$iterations"' * 100)}
  ' < "$results_directory/$payload.csv" >> "$results_directory/success.csv"

	# Filter on 200s, convery from s to ms, and sort
	awk -F, '$7 == 200 {print ($1 * 1000)}' < "$results_directory/$payload.csv" \
		| sort -g > "$results_directory/$payload-response.csv"

	# Get Number of 200s
	oks=$(wc -l < "$results_directory/$payload-response.csv")
	((oks == 0)) && continue # If all errors, skip line

	# Get Latest Timestamp
	duration=$(tail -n1 "$results_directory/$payload.csv" | cut -d, -f8)
	throughput=$(echo "$oks/$duration" | bc)
	printf "%d,%f\n" "$payload" "$throughput" >> "$results_directory/throughput.csv"

	# Generate Latency Data for csv
	awk '
    BEGIN {
      sum = 0
      p50 = int('"$oks"' * 0.5)
      p90 = int('"$oks"' * 0.9)
      p99 = int('"$oks"' * 0.99)
      p100 = '"$oks"'
      printf "'"$payload"',"
    }   
    NR==p50 {printf "%1.4f,", $0}
    NR==p90 {printf "%1.4f,", $0}
    NR==p99 {printf "%1.4f,", $0}
    NR==p100 {printf "%1.4f\n", $0}
  ' < "$results_directory/$payload-response.csv" >> "$results_directory/latency.csv"

	# Delete scratch file used for sorting/counting
	rm -rf "$results_directory/$payload-response.csv"
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
