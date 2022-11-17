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
cores=12
#sledge_num=(1 2 3 4 5 6)
sledge_num=(6)
ulimit -n 1000000 
./kill_sledge.sh
for(( i=0;i<${#sledge_num[@]};i++ )) do
	echo ${sledge_num[i]}
	for((j=1;j<=${sledge_num[i]};j++)); 
	do
		start_script_name="start"$j".sh"
		server_log="server-"${sledge_num[i]}"_$cores""-$fib_num-$concurrency"$j".log"
		./$start_script_name $cores > $server_log 2>&1 &
		echo $start_script_name
		echo $server_log

	done
done

