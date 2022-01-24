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

if ! command -v loadtest > /dev/null; then

	if ! command -v npm > /dev/null; then
		if [[ $(whoami) == "root" ]]; then
			apt update
			apt install npm
		else
			sudo apt update
			sudo apt install npm
		fi
	fi

	if [[ $(whoami) == "root" ]]; then
		npm install -g loadtest
	else
		sudo npm install -g loadtest
	fi
fi

if ! command -v gnuplot > /dev/null; then

	if [[ $(whoami) == "root" ]]; then
		apt-get update
		apt-get install gnuplot
	else
		sudo apt-get update
		sudo apt-get install gnuplot
	fi
fi
