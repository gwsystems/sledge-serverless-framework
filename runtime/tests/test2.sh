#hey -c $concurrency -z $duration\m -disable-keepalive -m GET -d 5 "http://127.0.0.1:10000" > $output
#hey -c 50 -n 100 -disable-keepalive -m GET -d 6 "http://127.0.0.1:10000" > $output
hey -disable-compression -disable-keepalive -disable-redirects -c 10 -z 1\m -cpus 1 -t 0 -m GET -D "5k.jpg" "http://10.10.1.1:10000"
