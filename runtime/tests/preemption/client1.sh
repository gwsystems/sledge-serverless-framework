#!/bin/bash
# fib(20)
# Perhaps this can be improved to pass a body without an additional file
ab -n 100000 -c 64 -s 999999999 -p client1_body.txt -v 4 -r localhost:10000/
