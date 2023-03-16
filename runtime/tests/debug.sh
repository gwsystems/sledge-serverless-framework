#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

declare project_path="$(
	cd "$(dirname "$1")/../.."
	pwd
)"
path=`pwd`
echo $project_path
cd $project_path/runtime/bin
#export SLEDGE_DISABLE_PREEMPTION=true
export SLEDGE_SANDBOX_PERF_LOG=$path/srsf.log
export SLEDGE_NWORKERS=6
export SLEDGE_FIRST_WORKER_COREID=7
export SLEDGE_NLISTENERS=6
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"
#gdb --eval-command="handle SIGUSR1 nostop" \
#	--eval-command="set pagination off" \
#	--eval-command="set substitute-path /sledge/runtime $project_path/runtime" \
#	--eval-command="run ../tests/fib.json" 
#	./sledgert

gdb ./sledgert
