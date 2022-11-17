#!/bin/bash

function usage {
        echo "$0 [concurrency] [fib number]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

#cores_list=(1 2 4 6 8 10 20 30 40 50 60 70 80)
#cores_list=(1 2 4 6 8 10 20 30 40 50 60 70 77)
concurrency=$1
fib_num=$2

echo "fib num is $fib_num"
#cores_list=(1 2 4 6 8 10 12 14 16 18 20 24 28 32 36 40 77)
cores=1
#sledge_num=(2)
ulimit -n 1000000 
./kill_sledge.sh


for((j=1;j<=12;j++));
do
	start_script_name="start_single_worker"$j".sh"
        server_log="server-"$cores"-$fib_num-$concurrency"$j".log"
        ./$start_script_name $cores > $server_log 2>&1 &
        echo $start_script_name
        echo $server_log

done
step=4
for((j=1;j<=12;j++));
do
	start_core=$(( 80 + ($j-1) * $step ))
	end_core=$(( 80 + $j * $step -1 ))
	echo $start_core $end_core
	port=$(( $j - 1 + 10030))
	taskset --cpu-list $start_core-$end_core hey -disable-compression -disable-keepalive -disable-redirects -z "60"s -c "$concurrency" -m POST -d "$fib_num" "http://127.0.0.1:$port/fib" > /dev/null 2>&1 &
done	

sleep 60
./kill_sledge.sh
folder_name=${sledge_num[i]}"_$fib_num""_c$concurrency"
mkdir $folder_name
#mv *.log $folder_name

