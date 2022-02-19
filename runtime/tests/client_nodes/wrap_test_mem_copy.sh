#!/bin/bash

function usage {
        echo "$0 [loop count]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi


loop_count=$1

for ((i=1; i <=$loop_count; i++))
do
	./test_mem_copy.sh chain 10004 $i
	#./test_mem_copy.sh single 10003 $i
 	#./cpu_insensive.sh chain 10009 $i
 	#./cpu_insensive.sh single 10003 $i
done

