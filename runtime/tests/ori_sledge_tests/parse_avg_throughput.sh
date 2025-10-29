#!/bin/bash

# 文件名匹配模式，例如 output_*.txt
files="output_*.txt"

# 初始化总和和计数
total=0
count=0

# 遍历每个文件
for file in $files; do
    if [ -f "$file" ]; then
        # 提取 Requests/sec 后的数字
        value=$(grep "Requests/sec:" "$file" | awk '{print $2}')
        if [[ $value =~ ^[0-9.]+$ ]]; then
            total=$(echo "$total + $value" | bc)
            count=$((count + 1))
        fi
    fi
done

# 计算平均值
if [ $count -gt 0 ]; then
    average=$(echo "scale=4; $total / $count" | bc)
    echo "Average Requests/sec: $average"
else
    echo "No valid data found."
fi
