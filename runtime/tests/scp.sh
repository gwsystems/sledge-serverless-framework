#!/bin/bash
 
if [ "$#" -lt 2 ]; then
  echo "Usage: $0 target_path file1 [file2 ... fileN]"
  exit 1
fi

TARGET_DIR="$1"
shift # remove first parameter, so $@ is the file list

PRIVATE_KEY="/my_mount/sledge-serverless-framework/runtime/tests/id_rsa"
TARGET_USER="xiaosuGW"
TARGET_HOST="10.10.1.2"

chmod 400 "$PRIVATE_KEY"

#loop copy 
for file in "$@"; do
  if [ -f "$file" ]; then
    scp -i "$PRIVATE_KEY" "$file" "$TARGET_USER@$TARGET_HOST:$TARGET_DIR"
    if [ $? -eq 0 ]; then
      echo "Successfully copied $file to $TARGET_DIR"
    else
      echo "Failed to copy $file to $TARGET_DIR"
    fi
  else
    echo "File $file does not exist, skipping."
  fi
done
