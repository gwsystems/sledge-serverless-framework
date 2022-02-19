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
	for ((j=1; j <=5; j++))
	do
		./noop.sh $j $i
	done
done
