#!/bin/bash


#concurrency_list=(50 100 200 300 400 1000)
concurrency_list=(200 300 400 1000)
for(( i=0;i<${#concurrency_list[@]};i++ )) do
	./test.sh ${concurrency_list[i]} 15 
done

