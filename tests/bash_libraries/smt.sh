#!/bin/sh

# This script can set the SMT is on or off. 


# Provides help to user on how to use this script
usage() {
  echo "usage $0 <check||on||off/>"
  echo "      check   Check SMT (hyperthreading) status"
  echo "      on      Enable SMT"
  echo "      off     Disable SMT"
}

# Check governor status
check() {
  echo "SMT status: (1 means ENABLED, 0 otherwise)"
  cat /sys/devices/system/cpu/smt/active
}

# Enable SMT
enable_smt() {
  echo on > /sys/devices/system/cpu/smt/control
}

# Disable SMT
disable_smt() {
  echo off > /sys/devices/system/cpu/smt/control
}

if [ $# -ne 1 ]; then
  #echo "incorrect number of arguments: $*"
  #usage "$0"
  #exit 1
  check
  exit 1
fi

case $1 in
  check)
    check
    ;;
  on)
    enable_smt
    ;;
  off)
    disable_smt
    ;;
  *)
    echo "invalid option: $1"
    usage "$0"
    exit 1
    ;;
esac

