function usage {
        echo "$0 [output file, chain_function_output.txt or single_function_output.txt] [concurrency] [duration(m)]"
        exit 1
}

if [ $# != 3 ] ; then
        usage
        exit 1;
fi

output=$1
concurrency=$2
duration=$3

hey -c $concurrency -z $durationm -disable-keepalive -m GET -d 5 "http://127.0.0.1:10000" > $output
#hey -c 50 -n 100 -disable-keepalive -m GET -d 6 "http://127.0.0.1:10000" > $output
