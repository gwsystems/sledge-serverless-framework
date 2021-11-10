function usage {
        echo "$0 [output file] [duration(s)] [concurrency] [image file] [port]"
        exit 1
}

if [ $# != 5 ] ; then
	echo "input parameters are not 5"
        usage
        exit 1;
fi

output=$1
duration=$2
concurrency=$3
image=$4
port=$5

#hey -disable-compression -disable-keepalive -disable-redirects -c 1 -q $rps -z $duration\s -cpus 1 -t 0 -m GET -D "$image" "http://10.10.1.1:$port" 
#hey -disable-compression -disable-keepalive -disable-redirects -c $concurrency -z 20s -t 0 -m GET -D "$image" "http://10.10.1.1:$port" 
hey -disable-compression -disable-keepalive -disable-redirects -c $concurrency -z $duration\s -t 0 -m GET -D "$image" "http://10.10.1.1:$port" > $output 

