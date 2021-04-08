#!/bin/bash
source ../common.sh

# This experiment is intended to document how the level of concurrent requests influence the latency, throughput, and success/failure rate
# Use -d flag if running under gdb

timestamp=$(date +%s)
experiment_directory=$(pwd)
binary_directory=$(cd ../../bin && pwd)

results_directory="$experiment_directory/res/$timestamp/$scheduler"
log=log.txt

mkdir -p "$results_directory"
log_environment >> "$results_directory/$log"

# Start the runtime
PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" | tee -a "$results_directory/$log"
