#!/bin/bash

expected_result="This is q handw__tten
examp(e for _0CR,
Write as good as yo_ c4n."

success_count=0
total_count=1000

for ((i = 0; i < total_count; i++)); do
  echo "$i"
  result=$(curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@handwrt1.pnm" localhost:10000 2>/dev/null)
  # echo "$result"
  if [[ "$result" == "$expected_result" ]]; then
    success_count=$((success_count + 1))
  else
    echo "FAIL"
    echo "Expected $expected_result"
    echo "Was $result"
  fi
done

echo "$success_count / $total_count"
