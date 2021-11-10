#!/bin/bash

function usage {
        echo "$0 [cpu-log]"
        exit 1
}


#basepath=$(cd `dirname $0`; pwd)
#cpu_log=$1

#if [ -z $cpu_log ]
#then
#    usage
#fi


#node1_cpu_file=$cpu_log"_node1_cpu.log"

chmod 400 ./id_rsa
path="/users/xiaosuGW/sledge-serverless-framework/runtime/tests"

#test c3
server_log_file="edf_15.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 15
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c5
server_log_file="edf_22.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 22
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c7
server_log_file="edf_30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 30 
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c10
server_log_file="edf_38.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 38 
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"
