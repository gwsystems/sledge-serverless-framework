#!/bin/bash
function usage {
        echo "$0 [repeat count]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

chmod 400 ./id_rsa
remote_ip="10.10.1.1"

repeat_count=$1
> old_sledge.log

path="/my_mount/ori_sledge/sledge-serverless-framework/runtime/tests"
for(( i=0;i<$repeat_count;i++ )) do
	echo "i is $i"
	echo "start server..."
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "sudo $path/start.sh > 1.txt 2>&1 &"
        sleep 1 
	echo "start client..."
        ./curl.sh >> old_sledge.log
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "sudo $path/kill_sledge.sh"
done
