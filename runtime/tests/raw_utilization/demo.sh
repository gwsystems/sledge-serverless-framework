#!/bin/bash
# cd ../../bin
# LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/mixed_preemption/test_mixed_preemption.json &
# cd ../tests/mixed_preemption/

# Run small samples on each port to let the runtime figure out the execution time
sleep 10
echo "Running Samples"
wrk -d 1m -t 1 -s post.lua http://localhost:10025 -- --delay 0 25\n

# Run in Serial, increasing number of open connections
sleep 10
echo "Running Experiments"
wrk --duration 1m --threads 1 --connections 10 --timeout 10m --script post.lua http://localhost:10025 -- --delay 0 25\n >./res/con10.txt
wrk --duration 1m --threads 1 --connections 15 --timeout 10m --script post.lua http://localhost:10025 -- --delay 0 25\n >./res/con15.txt
wrk --duration 1m --threads 1 --connections 20 --timeout 10m --script post.lua http://localhost:10025 -- --delay 0 25\n >./res/con20.txt
wrk --duration 1m --threads 1 --connections 25 --timeout 10m --script post.lua http://localhost:10025 -- --delay 0 25\n >./res/con25.txt

# Kill the Background Sledge processes
sleep 10
echo "Running Cleanup"
pkill sledgert
pkill wrk

# Extract the Latency CSV Data from the Log

echo 'con10, con10' >./res/con10.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/con10.txt >>./res/con10.csv

echo 'con15, con15' >./res/con15.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/con15.txt >>./res/con15.csv

echo 'con20, con20' >./res/con20.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/con20.txt >>./res/con20.csv

echo 'con25, con25' >./res/con25.csv
grep -A200 -m1 -e 'Percentile, Latency' ./res/con25.txt >>./res/con25.csv

paste -d, ./res/con10.csv ./res/con15.csv ./res/con20.csv ./res/con25.csv >./res/merged.csv
