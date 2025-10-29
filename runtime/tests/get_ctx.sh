#!/bin/bash

function usage {
        echo "$0 [concurrency]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

concurrency=$1
output="${concurrency}_ctx.txt"
pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`
total_switches=0

if [ -z "$pid" ] || [ ! -d "/proc/$pid" ]; then
    echo "PID $pid not found or no longer exists, skipping" | tee -a $output
    exit 1
fi

echo "PID $pid:" | tee -a $output

for tid in $(ls /proc/$pid/task); do
    thread_name=$(cat /proc/$pid/task/$tid/comm 2>/dev/null)

    if [ "$thread_name" = "worker_thread" ]; then
        if [ -f "/proc/$pid/task/$tid/sched" ]; then
            tid_switches=$(grep nr_switches /proc/$pid/task/$tid/sched | awk '{print $3}')
            echo "  TID $tid (name=$thread_name): nr_switches = $tid_switches" | tee -a $output
            total_switches=$((total_switches + tid_switches))
        fi
    fi
done

echo "----------------------------------------" | tee -a $output
echo "Total nr_switches for all 'worker_thread' threads: $total_switches" | tee -a $output
echo "Results saved to $output"
