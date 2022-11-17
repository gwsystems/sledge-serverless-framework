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
cores_list=(1 2 4 6 8 10 12 14 16 18 20 24 28 32 36 40 44 48 52 56 60 64 68 72 77)
#cores_list=(40)
#cores_list=(44 48 52 56 60 64 68 72)
ulimit -n 1000000 
./kill_sledge.sh
for(( i=0;i<${#cores_list[@]};i++ )) do
	hey_log=${cores_list[i]}"-$fib_num-$concurrency.log" #8-38-100
	server_log="server-"${cores_list[i]}"-$fib_num-$concurrency.log"
	#./start.sh ${cores_list[i]} >/dev/null 2>&1 & 
	./start.sh ${cores_list[i]} > $server_log 2>&1 & 
	#./start.sh ${cores_list[i]} > $server_log & 
        echo "sledge start with worker core ${cores_list[i]}"
	taskset --cpu-list 80-159 hey -disable-compression -disable-keepalive -disable-redirects -z "60"s -c "$concurrency" -m POST -d "$fib_num" "http://127.0.0.1:10030/fib" > $hey_log
	#taskset --cpu-list 80-159 hey -disable-compression -disable-keepalive -disable-redirects -z "60"s -c "$concurrency" "http://127.0.0.1:10030/fib" > $hey_log
	./kill_sledge.sh
done

folder_name="$fib_num""_c$concurrency"
mkdir $folder_name
mv *.log $folder_name
