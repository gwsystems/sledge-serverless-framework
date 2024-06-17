#!/bin/bash

if [ $# -ne 3 ]; then
  echo "Usage: $0 <filename> <number1> <number2>"
  exit 1
fi

FILENAME=$1
NUMBER1=$2
NUMBER2=$3

# Validate if file exists
if [ ! -f "$FILENAME" ]; then
  echo "File $FILENAME does not exist."
  exit 1
fi

# Replace the "n-resas" value in the 11th line with NUMBER1
sed -i "11s/\(\"n-resas\": \)[0-9]\+/\1$NUMBER1/" "$FILENAME"

# Replace the "n-resas" value in the 21st line with NUMBER2
sed -i "21s/\(\"n-resas\": \)[0-9]\+/\1$NUMBER2/" "$FILENAME"

echo "The file $FILENAME has been updated."

