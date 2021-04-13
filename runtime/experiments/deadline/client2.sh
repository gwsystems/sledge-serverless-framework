#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Use -d flag if running under gdb

host=192.168.1.13
# host=localhost
timestamp=$(date +%s)
experiment_directory=$(pwd)

results_directory="$experiment_directory/res/$timestamp"
log=log.txt

mkdir -p "$results_directory"
log_environment >> "$results_directory/$log"

inputs=(40 10)
duration_sec=30
offset=5

# Execute workloads long enough for runtime to learn excepted execution time
echo -n "Running Samples: "
for input in ${inputs[*]}; do
	hey -n 16 -c 4 -t 0 -o csv -m GET -d "$input\n" http://${host}:$((10000 + input))
done
echo "[DONE]"
sleep 5

echo "Running Experiments"

# Run lower priority first, then higher priority. The lower priority has offsets to ensure it runs the entire time the high priority is trying to run
hey -z $((duration_sec + 2 * offset))s -cpus 3 -c 200 -t 0 -o csv -m GET -d "40\n" http://${host}:10040 > "$results_directory/fib40-con.csv" &
sleep $offset
hey -z ${duration_sec}s -cpus 3 -c 200 -t 0 -o csv -m GET -d "10\n" http://${host}:10010 > "$results_directory/fib10-con.csv" &
sleep $((duration_sec + offset + 15))
sleep 30

# Generate *.csv and *.dat results
echo -n "Parsing Results: "

printf "Payload,Success_Rate\n" >> "$results_directory/success.csv"
printf "Payload,Throughput\n" >> "$results_directory/throughput.csv"
printf "Payload,p50,p90,p99,p100\n" >> "$results_directory/latency.csv"

deadlines_ms=(20 20000)
payloads=(fib10-con fib40-con)
durations_s=(30 40)

for ((i = 0; i < 2; i++)); do
	payload=${payloads[$i]}
	deadline=${deadlines_ms[$i]}
	duration=${durations_s[$i]}

	# Get Number of Requests
	requests=$(($(wc -l < "$results_directory/$payload.csv") - 1))
	((requests == 0)) && continue

	# Calculate Success Rate for csv
	awk -F, '
		$7 == 200 {denom++}
		$7 == 200 && ($1 * 1000) <= '"$deadline"' {ok++}
		END{printf "'"$payload"',%3.5f\n", (ok / denom * 100)}
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
	printf "%s,%f\n" "$payload" "$throughput" >> "$results_directory/throughput.csv"

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
		NR==p50  {printf "%1.4f,",  $0}
		NR==p90  {printf "%1.4f,",  $0}
		NR==p99  {printf "%1.4f,",  $0}
		NR==p100 {printf "%1.4f\n", $0}
    ' < "$results_directory/$payload-response.csv" >> "$results_directory/latency.csv"

	# Delete scratch file used for sorting/counting
	# rm -rf "$results_directory/$payload-response.csv"
done

# Transform csvs to dat files for gnuplot
for file in success latency throughput; do
	echo -n "#" > "$results_directory/$file.dat"
	tr ',' ' ' < "$results_directory/$file.csv" | column -t >> "$results_directory/$file.dat"
done

# Generate gnuplots. Commented out because we don't have *.gnuplots defined
# generate_gnuplots

# Cleanup, if requires
echo "[DONE]"
