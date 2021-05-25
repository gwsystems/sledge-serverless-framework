#!/bin/bash

command -v imagemagick && {
	echo "imagemagick is already installed."
	return 0
}

# Installs the deps needed for run.sh
if [ "$(whoami)" == "root" ]; then
	apt-get install imagemagick
else
	sudo apt-get install imagemagick
fi
