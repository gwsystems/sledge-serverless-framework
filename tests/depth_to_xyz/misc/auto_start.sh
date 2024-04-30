#!/bin/bash

SLEDGE_CMUSOD_DIR="/users/emil/sledge-server/tests/cmu-sod"
# SLEDGE_CMUSOD_DIR="/home/gwu/sledge/tests/cmu-sod"

pidof  sledgert >/dev/null
if [[ $? -ne 0 ]] ; then
		now=$(date)
		echo "" >> $SLEDGE_CMUSOD_DIR/server_log.txt
		echo "Restarting Sledge:     $now" >> $SLEDGE_CMUSOD_DIR/server_log.txt
        make -C $SLEDGE_CMUSOD_DIR run &>> $SLEDGE_CMUSOD_DIR/server_log.txt &
fi
