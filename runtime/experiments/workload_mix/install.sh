#!/bin/bash

if ! command -v hey > /dev/null; then
	HEY_URL=https://hey-release.s3.us-east-2.amazonaws.com/hey_linux_amd64
	wget $HEY_URL -O hey
	chmod +x hey

	if [[ $(whoami) == "root" ]]; then
		mv hey /usr/bin/hey
	else
		sudo mv hey /usr/bin/hey
	fi
fi
