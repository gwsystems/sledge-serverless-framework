#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
runtime_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$runtime_directory"/bin && pwd)
log="$experiment_directory/log.csv"

# Copy data if not here
if [[ ! -f "./initial_state.dat" ]]; then
  cp $runtime_directory/tests/TinyEKF/extras/c/ekf_raw.dat ./initial_state.dat
fi

if [ "$1" != "-d" ]; then
  SLEDGE_SANDBOX_PERF_LOG=$log PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" >rt.log 2>&1 &
  sleep 2
else
  echo "Running under gdb"
fi

one_iteration_expected_result="$(tr -d '\0' <./one_iteration.dat)"
two_iterations_expected_result="$(tr -d '\0' <./two_iterations.dat)"
three_iterations_expected_result="$(tr -d '\0' <./three_iterations.dat)"

success_count=0
total_count=50

for ((i = 0; i < total_count; i++)); do
  echo "$i"
  curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@initial_state.dat" localhost:10000 2>/dev/null >./one_iteration_res.dat
  curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@one_iteration_res.dat" localhost:10001 2>/dev/null >./two_iterations_res.dat
  curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@two_iterations_res.dat" localhost:10002 2>/dev/null >./three_iterations_res.dat
  if diff -s one_iteration_res.dat one_iteration.dat && diff -s two_iterations_res.dat two_iterations.dat && diff -s three_iterations_res.dat three_iterations.dat; then
    success_count=$((success_count + 1))
  else
    echo "FAIL"
    exit
  fi

  rm *_res.dat
done

echo "$success_count / $total_count"

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  echo "[DONE]"
fi
