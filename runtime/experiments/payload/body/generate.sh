#!/bin/bash
# Generates payloads of 1KB, 10KB, 100KB, 1MB
for size in 1024 $((1024 * 10)) $((1024 * 100)) $((1024 * 1024)); do
  rm -rf $size.txt
  i=0
  echo -n "Generating $size:"
  while ((i < size)); do
    printf 'a' >>$size.txt
    ((i++))
  done
  echo "[DONE]"
done
