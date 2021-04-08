#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
echo "$experiment_directory"
project_directory=$(cd ../../../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)
did_pass=true

# Copy data if not here
if [[ ! -f "./ekf_raw.dat" ]]; then
	cp ../../../tests/TinyEKF/extras/c/ekf_raw.dat ./ekf_raw.dat
fi

if [ "$1" != "-d" ]; then
	PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json" &
	sleep 1
else
	echo "Running under gdb"
fi

expected_result="$(tr -d '\0' < ./expected_result.dat)"

success_count=0
total_count=50

for ((i = 0; i < total_count; i++)); do
	echo "$i"
	result="$(curl -H 'Expect:' -H "Content-Type: application/octet-stream" --data-binary "@ekf_raw.dat" localhost:10000 2> /dev/null | tr -d '\0')"
	if [[ "$expected_result" == "$result" ]]; then
		success_count=$((success_count + 1))
	else
		echo "FAIL"
		did_pass=false
		break
	fi
done

echo "$success_count / $total_count"

if [ "$1" != "-d" ]; then
	sleep 5
	echo -n "Running Cleanup: "
	pkill sledgert > /dev/null 2> /dev/null
	echo "[DONE]"
fi

if $did_pass; then
	exit 0
else
	exit 1
fi
