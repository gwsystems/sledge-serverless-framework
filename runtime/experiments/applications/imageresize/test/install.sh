#!/bin/bash

# Installs the deps needed for run.sh
if [ "$(whoami)" == "root" ]; then
  apt-get install imagemagick
else
  sudo apt-get install imagemagick
fi
