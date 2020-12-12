#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
project_directory=$(cd ../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)

if [ "$1" != "-d" ]; then
  PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" &
  sleep 1
else
  echo "Running under gdb"
fi

expected_result="$(cat ./expected_result.txt)"
success_count=0
total_count=50

for ((i = 0; i < total_count; i++)); do
  echo "$i"
  result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@hyde.pnm" localhost:10000 2>/dev/null)
  # echo "$result"
  if [[ "$result" == "$expected_result" ]]; then
    success_count=$((success_count + 1))
  else
    echo "FAIL"
    echo "Expected:"
    echo "$expected_result"
    echo "==============================================="
    echo "Was:"
    echo "$result"
  fi
done

echo "$success_count / $total_count"

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  echo "[DONE]"
fi
