#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <num_copies>"
    exit 1
fi

num_copies=$1
func_name="fibonacci"

for i in $(seq $num_copies); do
    cp $func_name".wasm.so" $func_name$i.wasm.so
done

echo "Files copied successfully."

