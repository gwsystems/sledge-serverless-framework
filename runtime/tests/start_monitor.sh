#!/bin/bash

function usage {
        echo "$0 [vmstat file] [pidstat file]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

vmstat_file=$1
pidstat_file=$2

sledge_pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`
vmstat 1 > $vmstat_file 2>&1 &
pidstat -w 1 150 -p $sledge_pid > $pidstat_file 2>&1 &
