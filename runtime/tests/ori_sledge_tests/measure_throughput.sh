#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <number_of_runs>"
    exit 1
fi

num_runs=$1
chmod 400 ./id_rsa
remote_ip="128.110.218.253"

ulimit -n 655350
path="/my_mount/old_sledge/sledge-serverless-framework/runtime/tests"

for i in $(seq 1 $num_runs); do
    ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/start.sh > 1.txt 2>&1 &"
    sleep 5
    echo "Running test $i..."
    hey -disable-compression -disable-redirects -cpus 20 -z "20"s -c "300" -m POST "http://10.10.1.1:31850/empty" > "output_$i.txt"
    ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "sudo $path/kill_sledge.sh" 
done

echo "All tests completed."
