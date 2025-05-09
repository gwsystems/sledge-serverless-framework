#!/bin/bash
ulimit -n 655350

function usage {
        echo "$0 [worker num] [listener num] [first worker core id] [dispatcher policy, SHINJUKU, EDF_INTERRUPT, DARC or TO_GLOBAL_QUEUE] [scheduler policy, EDF or FIFO] [server log file] [disable busy loop] [disable autoscaling] [disable service time simulation] [json config]"
        exit 1
}

if [ $# != 10 ] ; then
        usage
        exit 1;
fi

worker_num=$1
listener_num=$2
first_worker_core_id=$3
dispatcher_policy=$4
scheduler_policy=$5
server_log=$6
disable_busy_loop=$7
disable_autoscaling=$8
disable_service_ts_simulation=$9
json_config=${10}

if [ "$scheduler_policy" = "FIFO" ]; then
    worker_group_size=1
else
    worker_group_size=$((worker_num / listener_num))
fi

declare project_path="$(
        cd "$(dirname "$0")/../.."
        pwd
)"
echo $project_path
path=`pwd`
export SLEDGE_DISABLE_PREEMPTION=true
export SLEDGE_DISABLE_BUSY_LOOP=$disable_busy_loop
export SLEDGE_DISABLE_AUTOSCALING=$disable_autoscaling
#export SLEDGE_SIGALRM_HANDLER=TRIAGED
export SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION=$disable_service_ts_simulation
export SLEDGE_FIRST_WORKER_COREID=$first_worker_core_id
export SLEDGE_NWORKERS=$worker_num
export SLEDGE_NLISTENERS=$listener_num
export SLEDGE_WORKER_GROUP_SIZE=$worker_group_size
export SLEDGE_SCHEDULER=$scheduler_policy
#export SLEDGE_DISPATCHER=DARC
export SLEDGE_DISPATCHER=$dispatcher_policy
export SLEDGE_SCHEDULER=$scheduler_policy
#export SLEDGE_DISPATCHER=EDF_INTERRUPT
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
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/hash.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/empty.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/fib.json
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/$json_config
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_sodresize.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_sodresize.json

