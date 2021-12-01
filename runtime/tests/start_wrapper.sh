#!/bin/bash

function usage {
        echo "$0 [server-log]"
        exit 1
}

server_log=$1

if [ -z $server_log ]
then
    usage
fi

./start.sh $server_log >/dev/null 2>&1 &

