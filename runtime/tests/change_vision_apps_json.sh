#!/bin/bash

# 检查参数数量
if [ $# -ne 3 ]; then
  echo "Usage: $0 <multiplier> <file_path> <request_type>"
  exit 1
fi

# 参数赋值
multiplier=$1
file_path=$2
request_type=$3

# 创建临时文件
temp_file=$(mktemp)

# 读取文件并修改内容
while IFS= read -r line; do
    if echo "$line" | grep -q "\"request-type\": $request_type,"; then
        found_request_type=1
    fi

    if [ "$found_request_type" == "1" ] && echo "$line" | grep -q "\"expected-execution-us\": "; then
        expected_exec_us=$(echo "$line" | grep -o "[0-9]\+")
    fi

    if [ "$found_request_type" == "1" ] && echo "$line" | grep -q "\"relative-deadline-us\": "; then
        new_deadline=$((expected_exec_us * multiplier))
        line=$(echo "$line" | sed -E "s/[0-9]+/$new_deadline/")
        found_request_type=0
    fi

    echo "$line" >> "$temp_file"
done < "$file_path"

# 替换原文件
mv "$temp_file" "$file_path"

echo "Updated 'relative-deadline-us' for request_type $request_type in $file_path."

