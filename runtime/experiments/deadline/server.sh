#!/bin/bash

experiment_directory=$(pwd)
binary_directory=$(cd ../../bin && pwd)

# Start the runtime

PATH="$binary_directory:$PATH" LD_LIBRARY_PATH="$binary_directory:$LD_LIBRARY_PATH" sledgert "$experiment_directory/spec.json"
