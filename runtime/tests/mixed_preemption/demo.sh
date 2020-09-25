#!/bin/bash
# cd ../../bin
# LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/mixed_preemption/test_mixed_preemption.json &
# cd ../tests/mixed_preemption/

# TODO: Run small samples on each port to let the runtime figure out the execution time
# ab -s 999999999 -v 4 -n 50 -c 1 -r -p ./req/fib10.txt localhost:10010/
# ab -s 999999999 -v 4 -n 50 -c 1 -r -p ./req/fib20.txt localhost:10020/
# ab -s 999999999 -v 4 -n 50 -c 1 -r -p ./req/fib25.txt localhost:10025/
# ab -s 999999999 -v 4 -n 50 -c 1 -r -p ./req/fib30.txt localhost:10030/
# ab -s 999999999 -v 4 -n 50 -c 1 -r -p ./req/fib35.txt localhost:10035/
wrk -d 10s -t1 -s post.lua http://localhost:10010 -- 2 10\n >/dev/null
# wrk -d 10s -t1 -s post.lua http://localhost:10030 -- 2 30\n >/dev/null
# wrk -d 1m -t1 -s post.lua http://localhost:10020 -- 2 20\n

# fib(10)
# sleep 2
# wrk -d 1m -t1 -s post.lua http://localhost:10010 -- 20 10\n >./res/fib10.txt
# wrk -d 1m -t1 -s post.lua http://localhost:10020 -- 2 20\n >./res/fib20.txt &
# wrk -d 1m -t1 -s post.lua http://localhost:10025 -- 2 25\n >./res/fib25.txt &
# wrk -d 1m -t1 -s post.lua http://localhost:10030 -- 2 30\n >./res/fib30.txt
# wrk -d 1m -t1 -s post.lua http://localhost:10035 -- 2 35\n
# ab -n 10000 -c 1 -r -p ./req/fib10.txt localhost:10000/ >./res/fib10.txt &
# ab -n 10000 -c 1 -r -p ./req/fib20.txt localhost:10001/ >./res/fib20.txt &
# ab -n 10000 -c 1 -r -p ./req/fib25.txt localhost:10002/ >./res/fib25.txt &
# ab -n 10000 -c 1 -r -p ./req/fib30.txt localhost:10030/ >./res/fib30.txt &
# ab -n 10000 -c 1 -r -p ./req/fib35.txt localhost:10035/ >./res/fib35.txt

# Kill the Background Sledge processes
# ps -e -o pid,cmd | grep sledgert | grep json | cut -d\  -f 1 | xargs kill
# pkill sledgert
# pkill ab
# pkill wrk
