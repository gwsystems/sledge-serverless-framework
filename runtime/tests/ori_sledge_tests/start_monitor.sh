#!/bin/bash

function usage {
        echo "$0 [pidstat file]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

pidstat_file=$1
sledge_pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`
sleep 6 && pidstat -u -p $sledge_pid 1 1800 > $pidstat_file 2>&1 &
