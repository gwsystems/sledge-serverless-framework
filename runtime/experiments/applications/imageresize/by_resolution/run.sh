#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
project_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)
log="$experiment_directory/log.csv"

if [ "$1" != "-d" ]; then
  SLEDGE_SANDBOX_PERF_LOG=$log PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" &
  sleep 1
else
  echo "Running under gdb"
fi

success_count=0
total_count=100

for ((i = 0; i < total_count; i++)); do
  echo "$i"
  ext="$RANDOM"

  curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_small.jpg" --output "result_${ext}_small.png" localhost:10000 2>/dev/null 1>/dev/null
  pixel_differences="$(compare -identify -metric AE "result_${ext}_small.png" expected_result_small.png null: 2>&1 >/dev/null)"
  if [[ "$pixel_differences" != "0" ]]; then
    echo "Small FAIL"
    echo "$pixel_differences pixel differences detected"
    exit
  fi

  curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_medium.jpg" --output "result_${ext}_medium.png" localhost:10001 2>/dev/null 1>/dev/null
  pixel_differences="$(compare -identify -metric AE "result_${ext}_medium.png" expected_result_medium.png null: 2>&1 >/dev/null)"
  if [[ "$pixel_differences" != "0" ]]; then
    echo "Small FAIL"
    echo "$pixel_differences pixel differences detected"
    exit
  fi

  curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@shrinking_man_large.jpg" --output "result_${ext}_large.png" localhost:10002 2>/dev/null 1>/dev/null
  pixel_differences="$(compare -identify -metric AE "result_${ext}_large.png" expected_result_large.png null: 2>&1 >/dev/null)"
  if [[ "$pixel_differences" != "0" ]]; then
    echo "Small FAIL"
    echo "$pixel_differences pixel differences detected"
    exit
  fi

  success_count=$((success_count + 1))
done

echo "$success_count / $total_count"
rm -f result_*.png

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  echo "[DONE]"
fi
