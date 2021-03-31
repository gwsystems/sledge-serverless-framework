#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
echo "$experiment_directory"
project_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)
log="$experiment_directory/log.csv"

if [ "$1" != "-d" ]; then
  SLEDGE_SANDBOX_PERF_LOG=$log PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" &
  sleep 1
else
  echo "Running under gdb"
fi

one_plate=(Cars0 Cars1 Cars2 Cars3 Cars4)
two_plates=(Cars71 Cars87 Cars143 Cars295 Cars316)
four_plates=(Cars106 Cars146 Cars249 Cars277 Cars330)

for image in ${one_plate[*]}; do
  echo "@./1/${image}.png"
  curl --data-binary "@./1/${image}.png" --output - localhost:10000
done
for image in ${two_plates[*]}; do
  echo "@./2/${image}.png"
  curl --data-binary "@./2/${image}.png" --output - localhost:10001
done
for image in ${four_plates[*]}; do
  echo "@./4/${image}.png"
  curl --data-binary "@./4/${image}.png" --output - localhost:10002
done

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  echo "[DONE]"
fi
