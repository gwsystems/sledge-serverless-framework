#!/bin/bash
# fib(10)
# Perhaps this can be improved to pass a body without an additional file
ab -n 100000 -c 100 -p client2_body.txt -v 4 -r localhost:10001/
