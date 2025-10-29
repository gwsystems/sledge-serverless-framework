#!/bin/bash

function usage {
        echo "$0 [output file]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

output=$1

pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`

if [ -z "$pid" ]; then
    echo "No process found. Exiting."
    exit 1
fi

interval=1  # 采样间隔时间，单位为秒
samples=20  # 采样次数

total_rss=0
count=0

while [ $count -lt $samples ]; do
    # 使用ps命令获取RSS (单位为KB)
    rss=$(ps -o rss= -p $pid)

    if [ -n "$rss" ]; then
        total_rss=$((total_rss + rss))
        count=$((count + 1))
    else
        echo "process $pid not exists or exits"
        exit 1
    fi

    sleep $interval  # 采样间隔
done

# 计算平均RSS
average_rss=$((total_rss / samples))

echo "avg RSS (KB): $average_rss" > $output

