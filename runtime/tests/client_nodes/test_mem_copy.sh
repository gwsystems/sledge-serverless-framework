#!/bin/bash
function usage {
        echo "$0 [prefix] [port] [times] "
        exit 1
}

if [ $# != 3 ] ; then
        usage
        exit 1;
fi


prefix=$1
port=$2
time=$3

path="/users/xiaosuGW/sledge-serverless-framework/runtime/tests"
chmod 400 $path/id_rsa

input=0
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"

hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=1
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=2
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=4
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=6
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=8
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

input=10
server_log="mem_copy_"$input".log"
ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log >/dev/null 2>&1 &"
hey -c 1 -z 60s -disable-keepalive -m GET -d $input "http://10.10.1.1:$port" > $prefix"_"$input"-"$time.txt

ssh -o stricthostkeychecking=no -i $path/id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


