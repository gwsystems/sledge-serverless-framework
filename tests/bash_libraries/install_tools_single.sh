#!/bin/bash

if ! command -v http > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y httpie
	else
		sudo apt update
		sudo apt install -y httpie
	fi
fi

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
			apt install -y npm
		else
			sudo apt update
			sudo apt install -y npm
		fi
	fi

	# Try pulling Emil's version of loadtest to support post binary files
	# if [[ $(whoami) == "root" ]]; then
	# 	npm install -y -g loadtest
	# else
	# 	sudo npm install -y -g loadtest
	# fi

	pushd ~
	git clone https://github.com/emil916/loadtest.git
	pushd loadtest
	if [[ $(whoami) == "root" ]]; then
		npm install -g
	else
		sudo npm install -g
	fi
	popd
	popd 
fi

if ! command -v gnuplot > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt-get update
		apt-get install -y gnuplot
	else
		sudo apt-get update
		sudo apt-get install -y gnuplot
	fi
fi


if ! command -v jq > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y jq
	else
		sudo apt update
		sudo apt install -y jq
	fi
fi

if ! command -v htop > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y htop
	else
		sudo apt update
		sudo apt install -y htop
	fi
fi