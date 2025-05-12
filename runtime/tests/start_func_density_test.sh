#!/bin/bash

ulimit -n 655350
ulimit -s unlimited
ulimit -m unlimited
sudo sysctl -w vm.max_map_count=262144

function usage {
        echo "$0 [dispatcher policy:SHINJUKU, EDF_INTERRUPT, DARC or TO_GLOBAL_QUEUE] [scheduler policy: EDF or FIFO] [disable busy loop] [worker num] [listener num] [first worker core id] [json file] [server log]"
        exit 1
}

if [ $# != 8 ] ; then
        usage
        exit 1;
fi

dispatcher_policy=$1
scheduler_policy=$2
disable_busy_loop=$3
worker_num=$4
listener_num=$5
first_worker_core_id=$6
json_file=$7
server_log=$8
worker_group_size=$((worker_num / listener_num))

declare project_path="$(
        cd "$(dirname "$0")/../.."
        pwd
)"
echo $project_path
path=`pwd`
export SLEDGE_DISABLE_PREEMPTION=true
export SLEDGE_DISABLE_BUSY_LOOP=$disable_busy_loop
export SLEDGE_DISABLE_AUTOSCALING=true
export SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION=true
#export SLEDGE_SIGALRM_HANDLER=TRIAGED
export SLEDGE_FIRST_WORKER_COREID=$first_worker_core_id
export SLEDGE_NWORKERS=$worker_num
export SLEDGE_NLISTENERS=$listener_num
export SLEDGE_WORKER_GROUP_SIZE=$worker_group_size
export SLEDGE_SCHEDULER=$scheduler_policy
#export SLEDGE_DISPATCHER=DARC
#export SLEDGE_DISPATCHER=SHINJUKU
export SLEDGE_DISPATCHER=$dispatcher_policy
export SLEDGE_SANDBOX_PERF_LOG=$path/$server_log
#echo $SLEDGE_SANDBOX_PERF_LOG
cd $project_path/runtime/bin
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_big_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_armcifar10.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_png2bmp.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_image_processing.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/mulitple_linear_chain.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_multiple_image_processing.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_multiple_image_processing3.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/fib.json
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/$json_file
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/hash.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/empty.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_sodresize.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_sodresize.json

