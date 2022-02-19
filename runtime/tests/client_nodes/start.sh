#!/bin/bash

pushd loadtest
./one_shoot.sh
popd

pushd srsf_loadtest
./one_shoot.sh
popd


pushd loadtest_p80
./one_shoot.sh
popd

