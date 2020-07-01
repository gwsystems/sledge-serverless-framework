#!/bin/bash
# Executes the runtime in GDB with SIGU

cd ../../bin
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"
gdb --eval-command="handle SIGUSR1 nostop" --eval-command="run ../tests/preemption/test_fibonacci_multiple.json" ./awsmrt
cd ../../tests
