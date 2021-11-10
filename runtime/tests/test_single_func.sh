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
f1="srsf_single1.txt"
server_log_file="srsf_single1.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 1 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"


#test single c10
f1="srsf_single5.txt"
server_log_file="srsf_single5.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 5 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single10.txt"
server_log_file="srsf_single10.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 10 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single20.txt"
server_log_file="srsf_single20.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 20 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single30.txt"
server_log_file="srsf_single30.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 30 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single40.txt"
server_log_file="srsf_single40.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 40 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single50.txt"
server_log_file="srsf_single50.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 50 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c10
f1="srsf_single60.txt"
server_log_file="srsf_single60.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 60 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c30
f1="srsf_single70.txt"
server_log_file="srsf_single70.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 70 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"

#test single c40
f1="srsf_single80.txt"
server_log_file="srsf_single80.log"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
./test_8c.sh $f1 120 80 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"
                                                                                                                                    
