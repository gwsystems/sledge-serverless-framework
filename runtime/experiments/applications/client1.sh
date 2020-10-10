#!/bin/bash

for i in {1..10000}; do
  echo "$i"
  curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@handwrt1.pnm" localhost:10000
done
