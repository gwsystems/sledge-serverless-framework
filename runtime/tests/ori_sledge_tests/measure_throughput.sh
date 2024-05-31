ulimit -n 655350
hey -disable-compression -disable-redirects -cpus 15 -z "20"s -c "400" -m POST "http://10.10.1.1:31850/empty"
