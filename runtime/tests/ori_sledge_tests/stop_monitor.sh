#!/bin/bash

pid=`ps -ef|grep  "pidstat"|grep -v grep |awk '{print $2}'`
echo $pid
kill -2 $pid
