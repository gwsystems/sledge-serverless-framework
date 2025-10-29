#!/bin/bash

pid=`ps -ef | grep "perf" | grep -v grep | awk 'NR==1 {print $2}'`
sudo kill -SIGINT $pid
sleep 2

pid=`ps -ef|grep  "perf" |grep -v grep |awk '{print $2}'`
echo $pid
sudo kill -9 $pid
