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


############################################################
#test single c10
f1="105k_edf_single_10.txt"
server_log_file="execution_edf_single_105k_10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 10 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="105k_edf_single_20.txt"
server_log_file="execution_edf_single_105k_20.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 20 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c30
f1="105k_edf_single_30.txt"
server_log_file="execution_edf_single_105k_30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 30 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c40
f1="105k_edf_single_40.txt"
server_log_file="execution_edf_single_105k_40.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 40 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"



#####################################################
#test single c10
f1="305k_edf_single_10.txt"
server_log_file="execution_edf_single_305k_10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 10 305k.jpg 10003
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


#test single c10
f1="305k_edf_single_20.txt"
server_log_file="execution_edf_single_305k_20.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 20 305k.jpg 10003
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c30
f1="305k_edf_single_30.txt"
server_log_file="execution_edf_single_305k_30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 30 305k.jpg 10003
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c40
f1="305k_edf_single_40.txt"
server_log_file="execution_edf_single_305k_40.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 40 305k.jpg 10003
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"



###############################################################
#test single c10
f1="5k_edf_single_10.txt"
server_log_file="execution_edf_single_5k_10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 10 5k.jpg 10006
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


#test single c10
f1="5k_edf_single_20.txt"
server_log_file="execution_edf_single_5k_20.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 20 5k.jpg 10006
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c30
f1="5k_edf_single_30.txt"
server_log_file="execution_edf_single_5k_30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 30 5k.jpg 10006
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c40
f1="5k_edf_single_40.txt"
server_log_file="execution_edf_single_5k_40.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 40 5k.jpg 10006
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


##################################################################
#test single c10
f1="40k_edf_single_10.txt"
server_log_file="execution_edf_single_40k_10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 10 40k.jpg 10009
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


#test single c10
f1="40k_edf_single_20.txt"
server_log_file="execution_edf_single_40k_20.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 20 40k.jpg 10009
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c30
f1="40k_edf_single_30.txt"
server_log_file="execution_edf_single_40k_30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 30 40k.jpg 10009
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c40
f1="40k_edf_single_40.txt"
server_log_file="execution_edf_single_40k_40.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 40 40k.jpg 10009
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

