#!/bin/bash

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Use -d flag if running under gdb

timestamp=$(date +%s)
experiment_directory=$(pwd)
binary_directory=$(cd ../../bin && pwd)
results_directory="$experiment_directory/res/$timestamp"
log=log.txt

mkdir -p "$results_directory"

{
  echo "*******"
  echo "* Git *"
  echo "*******"
  git log | head -n 1 | cut -d' ' -f2
  git status
  echo ""

  echo "************"
  echo "* Makefile *"
  echo "************"
  cat ../../Makefile
  echo ""

  echo "**********"
  echo "* Run.sh *"
  echo "**********"
  cat run.sh
  echo ""

  echo "************"
  echo "* Hardware *"
  echo "************"
  lscpu
  echo ""

  echo "*************"
  echo "* Execution *"
  echo "*************"
} >>"$results_directory/$log"

# Start the runtime
if [ "$1" != "-d" ]; then
  PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" >>"$results_directory/$log" 2>>"$results_directory/$log" &
  sleep 1
else
  echo "Running under gdb"
  echo "Running under gdb" >>"$results_directory/$log"
fi

iterations=10000

# Execute workloads long enough for runtime to learn excepted execution time
echo -n "Running Samples: "
hey -n "$iterations" -c 3 -q 200 -o csv -m GET http://localhost:10000
sleep 5
echo "[DONE]"

# Execute the experiments
concurrency=(1 20 40 60 80 100)
echo "Running Experiments"
for conn in ${concurrency[*]}; do
  printf "\t%d Concurrency: " "$conn"
  hey -n "$iterations" -c "$conn" -cpus 2 -o csv -m GET http://localhost:10000 >"$results_directory/con$conn.csv"
  echo "[DONE]"
done

# Stop the runtime

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  pkill wrk >/dev/null 2>/dev/null
  echo "[DONE]"
fi

# Generate *.csv and *.dat results
echo -n "Parsing Results: "

printf "Concurrency,Success_Rate\n" >>"$results_directory/success.csv"
printf "Concurrency,Throughput\n" >>"$results_directory/throughput.csv"
printf "Con,p50,p90,p99,p100\n" >>"$results_directory/latency.csv"

for conn in ${concurrency[*]}; do
  # Calculate Success Rate for csv
  awk -F, '
    $7 == 200 {ok++}
    END{printf "'"$conn"',%3.5f\n", (ok / '"$iterations"' * 100)}
  ' <"$results_directory/con$conn.csv" >>"$results_directory/success.csv"

  # Filter on 200s, convery from s to ms, and sort
  awk -F, '$7 == 200 {print ($1 * 1000)}' <"$results_directory/con$conn.csv" |
    sort -g >"$results_directory/con$conn-response.csv"

  # Get Number of 200s
  oks=$(wc -l <"$results_directory/con$conn-response.csv")
  ((oks == 0)) && continue # If all errors, skip line

  # Get Latest Timestamp
  duration=$(tail -n1 "$results_directory/con$conn.csv" | cut -d, -f8)
  throughput=$(echo "$oks/$duration" | bc)
  printf "%d,%f\n" "$conn" "$throughput" >>"$results_directory/throughput.csv"

  # Generate Latency Data for csv
  awk '
    BEGIN {
      sum = 0
      p50 = int('"$oks"' * 0.5)
      p90 = int('"$oks"' * 0.9)
      p99 = int('"$oks"' * 0.99)
      p100 = '"$oks"'
      printf "'"$conn"',"
    }   
    NR==p50 {printf "%1.4f,", $0}
    NR==p90 {printf "%1.4f,", $0}
    NR==p99 {printf "%1.4f,", $0}
    NR==p100 {printf "%1.4f\n", $0}
  ' <"$results_directory/con$conn-response.csv" >>"$results_directory/latency.csv"

  # Delete scratch file used for sorting/counting
  rm -rf "$results_directory/con$conn-response.csv"
done

# Transform csvs to dat files for gnuplot
for file in success latency throughput; do
  echo -n "#" >"$results_directory/$file.dat"
  tr ',' ' ' <"$results_directory/$file.csv" | column -t >>"$results_directory/$file.dat"
done

# Generate gnuplots
cd "$results_directory" || exit
gnuplot ../../latency.gnuplot
gnuplot ../../success.gnuplot
gnuplot ../../throughput.gnuplot
cd "$experiment_directory" || exit

# Cleanup, if requires
echo "[DONE]"
