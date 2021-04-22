#!/bin/bash

if ! command -v hey; then
	HEY_URL=https://hey-release.s3.us-east-2.amazonaws.com/hey_linux_amd64
	wget $HEY_URL -O hey && chmod +x hey && mv hey /usr/bin/hey
fi
