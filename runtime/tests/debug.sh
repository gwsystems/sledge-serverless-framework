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
export SLEDGE_DISABLE_BUSY_LOOP=false
export SLEDGE_DISABLE_AUTOSCALING=true
export SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION=true
export SLEDGE_DISABLE_PREEMPTION=true
export SLEDGE_SANDBOX_PERF_LOG=$path/server.log
export SLEDGE_NWORKERS=1
export SLEDGE_FIRST_WORKER_COREID=4
export SLEDGE_NLISTENERS=1
export SLEDGE_WORKER_GROUP_SIZE=1
export SLEDGE_SCHEDULER=FIFO
#export SLEDGE_DISPATCHER=DARC
#export SLEDGE_DISPATCHER=EDF_INTERRUPT
#export SLEDGE_DISPATCHER=SHINJUKU
export SLEDGE_DISPATCHER=TO_GLOBAL_QUEUE
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"
#gdb --eval-command="handle SIGUSR1 nostop" \
#	--eval-command="set pagination off" \
#	--eval-command="set substitute-path /sledge/runtime $project_path/runtime" \
#	--eval-command="run ../tests/fib.json" 
#	./sledgert

gdb --eval-command="handle SIGUSR1 nostop" \
    --eval-command="handle SIGUSR1 noprint" \
    ./sledgert
