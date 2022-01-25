#!/bin/bash

# Installs the deps needed for run.sh
if [ "$(whoami)" == "root" ]; then
	apt-get install netpbm pango1.0-tools wamerican
else
	sudo apt-get install netpbm pango1.0-tools wamerican
fi
