function usage {
        echo "$0 [output file, chain_function_output.txt or single_function_output.txt] [type, 1 means large image, 2 means small image] [rps] [concurrency] [duration(m)]"
        exit 1
}

if [ $# != 5 ] ; then
        usage
        exit 1;
fi

output=$1
image_type=$2
concurrency=$3
rps=$4
duration=$5

#hey -c $concurrency -z $duration\m -disable-keepalive -m GET -d 5 "http://127.0.0.1:10000" > $output
#hey -c 50 -n 100 -disable-keepalive -m GET -d 6 "http://127.0.0.1:10000" > $output
if [ $image_type == 1 ] ; then
	hey -disable-compression -disable-keepalive -disable-redirects -c $concurrency -q $rps -z $duration\m -cpus 1 -t 0 -m GET -D "dog_0340.jpg" "http://10.10.1.1:10000" > $output
	#hey -disable-compression -disable-keepalive -disable-redirects -c $concurrency -z $duration\m -cpus 1 -t 0 -m GET -D "dog_0340.jpg" "http://10.10.1.1:10000" > $output
else
	hey -disable-compression -disable-keepalive -disable-redirects -c $concurrency -q $rps -z $duration\m -cpus 1 -t 0 -m GET -D "motorbike_0431.jpg" "http://10.10.1.1:10003" > $output
fi
