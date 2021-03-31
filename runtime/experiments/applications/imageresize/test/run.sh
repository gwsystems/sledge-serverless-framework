#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
project_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)

# Copy Flower Image if not here
if [[ ! -f "./flower.jpg" ]]; then
  cp ../../../../tests/sod/bin/flower.jpg ./flower.jpg
fi

if [ "$1" != "-d" ]; then
  PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" &
  sleep 1
else
  echo "Running under gdb"
fi

success_count=0
total_count=10

for ((i = 0; i < total_count; i++)); do
  echo "$i"
  ext="$RANDOM"
  curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@flower.jpg" --output "result_$ext.png" localhost:10000 2>/dev/null 1>/dev/null

  pixel_differences="$(compare -identify -metric AE "result_$ext.png" expected_result.png null: 2>&1 >/dev/null)"

  if [[ "$pixel_differences" == "0" ]]; then
    success_count=$((success_count + 1))
  else
    echo "FAIL"
    echo "$pixel_differences pixel differences detected"
    exit 1
  fi
done

echo "$success_count / $total_count"
rm result_*.png

if [ "$1" != "-d" ]; then
  sleep 5
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  echo "[DONE]"
fi

exit 0
