#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

experiment_directory=$(pwd)
project_directory=$(cd ../.. && pwd)
binary_directory=$(cd "$project_directory"/bin && pwd)

export LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH"
export PATH="$binary_directory:$PATH"
export SLEDGE_SCHEDULER="EDF"

gdb --eval-command="handle SIGUSR1 nostop" \
	--eval-command="handle SIGPIPE nostop" \
	--eval-command="set pagination off" \
	--eval-command="set substitute-path /sledge/runtime $project_directory" \
	--eval-command="run $experiment_directory/spec.json" \
	sledgert
