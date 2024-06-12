#!/bin/bash

VARYING=(72 108 144)

for var in "${VARYING[@]}"; do
    mkdir -p "$var"dpi
    mkdir -p "$var"dpi-orig
    for ((i=5; i<=15; i++)); do
        shuf -n10 /usr/share/dict/american-english > "$var"dpi-orig/"$i"words.txt
        pango-view --dpi="$var" --font=mono -qo "$var"dpi-orig/"$var"dpi_"$i"words.png "$var"dpi-orig/"$i"words.txt
    done
done