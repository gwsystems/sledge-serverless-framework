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

SLEDGE_NWORKERS=5 SLEDGE_SCHEDULER=EDF perf record -g -s sledgert "$experiment_directory/spec.json"
