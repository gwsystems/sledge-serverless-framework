#!/bin/bash
cd ../../bin
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/mixed_preemption/test_mixed_preemption.json &
cd ../tests/mixed_preemption/

# Run small samples on each port to let the runtime figure out the execution time
sleep 10
echo "Running Samples"
wrk -d 20s -t1 -s post.lua http://localhost:10010 -- --delay 500 10\n
wrk -d 20s -t1 -s post.lua http://localhost:10020 -- --delay 500 20\n
wrk -d 20s -t1 -s post.lua http://localhost:10030 -- --delay 500 25\n

# Run in Parallel
sleep 10
echo "Running Experiments"
wrk -d 1m -t1 -s post.lua http://localhost:10010 -- --delay 125 10\n >./res/fib10.txt &
wrk -d 2m -t1 -s post.lua http://localhost:10020 -- --delay 250 20\n >./res/fib20.txt &
wrk -d 3m -t1 -s post.lua http://localhost:10025 -- --delay 500 25\n >./res/fib25.txt

# Kill the Background Sledge processes
sleep 10
echo "Running Cleanup"
pkill sledgert
pkill wrk

# Extract the Latency CSV Data from the Log

echo 'Fib10, Fib10' >./res/fib10.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/fib10.txt >>./res/fib10.csv
echo 'Fib20, Fib20' >./res/fib20.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/fib20.txt >>./res/fib20.csv
echo 'Fib25, Fib25' >./res/fib25.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/fib25.txt >>./res/fib25.csv
paste -d, ./res/fib10.csv ./res/fib20.csv ./res/fib25.csv >./res/merged.csv
