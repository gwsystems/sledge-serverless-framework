#!/bin/bash

pid=`ps -ef|grep  "vmstat"|grep -v grep |awk '{print $2}'`
echo $pid
kill -2 $pid

