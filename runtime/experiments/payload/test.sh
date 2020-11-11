#!/bin/bash

hey -n 100 -c 3 -q 100 -m GET -D "./body/1024.txt" http://localhost:10000
