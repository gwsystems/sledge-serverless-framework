#!/bin/bash

function usage {
        echo "$0 [output]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`
output=$1

cpu=$(lscpu | awk '/^Core\(s\) per socket:/ {cores=$4} /^Socket\(s\):/ {sockets=$2} END {print cores * sockets}')
last_core=$((cpu - 1)) #get the last core

#taskset -c $last_core sudo perf stat -B -e context-switches,cache-references,cache-misses,cycles,instructions,page-faults -p $pid -o $output 2>&1 &
#taskset -c $last_core sudo perf stat -e syscalls:sys_enter_mmap,syscalls:sys_enter_mprotect,syscalls:sys_enter_munmap,context-switches -p $pid -o $output 2>&1 &
taskset -c $last_core sudo perf stat -C 4 -e syscalls:sys_enter_mmap,syscalls:sys_enter_mprotect,syscalls:sys_enter_munmap,context-switches -o $output 2>&1 &

