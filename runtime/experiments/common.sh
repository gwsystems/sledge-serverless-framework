#!/bin/bash

log_environment() {
  echo "*******"
  echo "* Git *"
  echo "*******"
  git log | head -n 1 | cut -d' ' -f2
  git status
  echo ""

  echo "************"
  echo "* Makefile *"
  echo "************"
  cat ../../Makefile
  echo ""

  echo "**********"
  echo "* Run.sh *"
  echo "**********"
  cat run.sh
  echo ""

  echo "************"
  echo "* Hardware *"
  echo "************"
  lscpu
  echo ""

  echo "*************"
  echo "* Execution *"
  echo "*************"
}

kill_runtime() {
  echo -n "Running Cleanup: "
  pkill sledgert >/dev/null 2>/dev/null
  pkill hey >/dev/null 2>/dev/null
  echo "[DONE]"
}

generate_gnuplots() {
  cd "$results_directory" || exit
  gnuplot ../../latency.gnuplot
  gnuplot ../../success.gnuplot
  gnuplot ../../throughput.gnuplot
  cd "$experiment_directory" || exit
}
