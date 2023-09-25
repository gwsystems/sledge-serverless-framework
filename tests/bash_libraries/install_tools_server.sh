#!/bin/bash

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

# For SOD:
# if ! command -v imagemagick > /dev/null; then
# 	if [ "$(whoami)" == "root" ]; then
# 		apt-get install -y imagemagick
# 	else
# 		sudo apt-get install -y imagemagick
# 	fi
# fi

# For GOCR, too many to check one-by-one, so uncomment below to install:
# if [[ "$(whoami)" == "root" ]]; then
# 	apt-get install -y netpbm pango1.0-tools wamerican fonts-roboto fonts-cascadia-code fonts-dejavu
# else
# 	sudo apt-get install -y netpbm pango1.0-tools wamerican fonts-roboto fonts-cascadia-code fonts-dejavu
# fi
