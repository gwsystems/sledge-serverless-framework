#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

declare project_path="$(
	cd "$(dirname "$1")/../.."
	pwd
)"
echo $project_path
cd ../../bin
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"
gdb --eval-command="handle SIGUSR1 nostop" \
	--eval-command="set pagination off" \
	--eval-command="set substitute-path /sledge/runtime $project_path" \
	--eval-command="run ../tests/preemption/test_fibonacci_multiple.json" \
	./sledgert
cd ../../tests
