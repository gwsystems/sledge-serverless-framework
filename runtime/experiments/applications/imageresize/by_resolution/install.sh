#!/bin/bash

# Installs the deps needed for run.sh
if [[ $(whoami) != "root" ]]; then
  DEBIAN_FRONTEND=noninteractive sudo apt-get install imagemagick --yes
else
  DEBIAN_FRONTEND=noninteractive apt-get install imagemagick --yes
fi
