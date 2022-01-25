#!/bin/bash
# Generates payloads of 1KB, 10KB, 100KB, 1MB
for size in 1024 $((1024 * 10)) $((1024 * 100)) $((1024 * 1024)); do
	# If the file exists, but is not the right size, wipe it
	if [[ -f "$size.txt" ]] && (("$(wc -c "$size.txt" | cut -d\  -f1)" != size)); then
		rm -rf "$size.txt"
	fi

	# Regenerate the file if missing
	if [[ ! -f "$size.txt" ]]; then
		echo -n "Generating $size: "
		for ((i = 0; i < size; i++)); do
			printf 'a' >> $size.txt
		done
		echo "[OK]"
	fi

done
