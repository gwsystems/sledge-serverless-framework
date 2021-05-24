#!/bin/bash

declare image_buffer

if image_buffer <(curl -H 'Expect:' -H "Content-Type: image/jpg" --output "result_small.png" "localhost:10000"); then
	pixel_differences="$(compare -identify -metric AE "result_small.png" expected_result_small.png null: 2>&1 > /dev/null)"
	if [[ "$pixel_differences" != "0" ]]; then
		echo "Small FAIL"
		echo "$pixel_differences pixel differences detected"
	else
		echo "OK"
	fi
else
	echo "curl failed with ${?}. See man curl for meaning."
fi
