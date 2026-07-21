#!/bin/sh

# This script can check current cpu freq, scale it up or down. 


# Provides help to user on how to use this script
usage() {
  echo "usage $0 <check||up||down/>"
  echo "      check       Check governor status"
  echo "      up          Scale up (effective till next reboot)"
  echo "      down        Scale down (effective till next reboot)"
}
# Check governor status
check() {
  echo "CPU governor status: "
  cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
  echo "CPU current freqs: "
  cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
  echo "CPU available governors: "
  cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_available_governors
}

# Scale up
scale_up() {
  echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
}

# Scale down
scale_down() {
  echo powersave | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
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
  up)
    scale_up
    ;;
  down)
    scale_down
    ;;
  *)
    echo "invalid option: $1"
    usage "$0"
    exit 1
    ;;
esac

