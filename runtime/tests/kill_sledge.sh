#!/bin/bash

pid=`ps -ef|grep  "sledgert"|grep -v grep |awk '{print $2}'`
echo $pid
sudo kill -2 $pid

