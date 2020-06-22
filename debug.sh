#!/bin/bash
cd ./runtime/bin
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"
gdb --eval-command="handle SIGUSR1 nostop" --eval-command="run ../tests/test_fibonacci_multiple.json" ./awsmrt
cd ../..
