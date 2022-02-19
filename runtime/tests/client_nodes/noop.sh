#!/bin/bash
function usage {
        echo "$0 [chain length] [repeat times]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

chain_len=$1
repeat_t=$2

server_log="noop_"$chain_len".log"
log="noop"$chain_len"-"$repeat_t".txt"
start_script="start-noop"$chain_len".sh"
echo $start_script
path="/users/xiaosuGW/sledge-serverless-framework/runtime/tests"
chmod 400 $path/id_rsa

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/$start_script $server_log >/dev/null 2>&1 &"

hey -c 1 -z 60s -disable-keepalive -m GET -d 29 "http://10.10.1.1:10000" > $log 2>&1 &
pid1=$!
wait -f $pid1

printf "[OK]\n"

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


