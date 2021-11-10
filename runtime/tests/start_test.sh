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
server_log_file="execution_mix_srsf_3.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 3
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c5
server_log_file="execution_mix_srsf_5.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 5
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c7
server_log_file="execution_mix_srsf_7.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 7 
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test c10
server_log_file="execution_mix_srsf_10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
echo "sledge is running"
./hey_test_8c.sh 120 10 
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"
