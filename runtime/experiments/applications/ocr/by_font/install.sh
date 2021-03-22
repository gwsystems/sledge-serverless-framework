#!/bin/bash

# Installs the deps needed for run.sh
if [[ $(whoami) != "root" ]]; then
  DEBIAN_FRONTEND=noninteractive sudo apt-get install netpbm pango1.0-tools wamerican --yes
else
  DEBIAN_FRONTEND=noninteractive apt-get install netpbm pango1.0-tools wamerican --yes
fi
