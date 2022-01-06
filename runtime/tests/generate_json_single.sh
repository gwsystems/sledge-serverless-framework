#!/bin/bash

function usage {
        echo "$0 [func name index] [loop count]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

func_name_index=$1
func_name="w"$func_name_index
json_file="test_multiple_image_processing_single.json"
rm -rf $json_file
touch $json_file

base_exec_time=(49444 124753 10799 29602)
base_port=$((10000 + ($func_name_index - 1)*3))
base_m_index=1
func1_base_name="resize"
func2_base_name="png2bmp"
func3_base_name="cifar10_"

loop_count=$1

for ((i=1; i <=$loop_count; i++))
do
	for ((j=1; j <=1; j++)) 
	do
		new_deadline=$((${base_exec_time[$j-1]} * ($i+1)))
		func1_new_name=$func1_base_name$func_name_index
		func1_new_port=$base_port
		base_port=$(($base_port + 1))

		cat >> $json_file << EOF
{
  "active": true,
  "name": "$func1_new_name",
  "path": "resize_wasm.so",
  "port": $func1_new_port,
  "relative-deadline-us": $new_deadline,
  "argsize": 1,
  "http-req-headers": [],
  "http-req-content-type": "image/jpeg",
  "http-req-size": 1024000,
  "http-resp-headers": [],
  "http-resp-size": 1024000,
  "http-resp-content-type": "image/png"
},

EOF
		func2_new_name=$func2_base_name$func_name_index
		func2_new_port=$base_port
		base_port=$(($base_port + 1))
	
		cat >> $json_file << EOF
{
  "active": true,
  "name": "$func2_new_name",
  "path": "C-Image-Manip_wasm.so",
  "port": $func2_new_port,
  "relative-deadline-us": $new_deadline,
  "argsize": 1,
  "http-req-headers": [],
  "http-req-content-type": "image/png",
  "http-req-size": 4096000,
  "http-resp-headers": [],
  "http-resp-size": 4096000,
  "http-resp-content-type": "image/bmp"
},

EOF
		func3_new_name=$func3_base_name$func_name_index
		func3_new_port=$base_port		
		base_port=$(($base_port + 1))
		
		cat >> $json_file << EOF
{
  "active": true,
  "name": "$func3_new_name",
  "path": "cifar10_wasm.so",
  "port": $func3_new_port,
  "relative-deadline-us": $new_deadline,
  "argsize": 1,
  "http-req-headers": [],
  "http-req-content-type": "image/bmp",
  "http-req-size": 4096000,
  "http-resp-headers": [],
  "http-resp-size": 1024,
  "http-resp-content-type": "text/plain",
  "tail-module": true
},

EOF
		echo "$func1_new_name, $func1_new_port, $func2_new_name, $func2_new_port, $func3_new_name, $func3_new_port, $new_deadline"
		func_name_index=$(($func_name_index + 4))
	done
	base_port=$(($base_port + 9))
done

