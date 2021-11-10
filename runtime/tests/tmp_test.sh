chmod 400 ./id_rsa
path="/users/xiaosuGW/sledge-serverless-framework/runtime/tests"


#test single c10
f1="30cpucore_single40.txt"
server_log_file="30cpucore_single40.log"
vmstat_file="30cpucore_single1_vmstat.txt"
pidstat_file="30cpucore_single1_pidstat.txt"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start.sh $server_log_file >/dev/null 2>&1 &"
#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/start_monitor.sh $vmstat_file $pidstat_file"
./test_8c.sh $f1 120 40 105k.jpg 10000
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/kill_sledge.sh"
#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@10.10.1.1 "$path/stop_monitor.sh"




