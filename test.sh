#!/bin/bash
# Test Driver Script

# Awsm
cd awsm && cargo build --release && cd ..

# Sledge
cd runtime && make clean all && cd ..

# OCR Build
# FIXME: gocr incorectly reports up to date
cd runtime/tests && make -B clean gocr && cd ../..

# OCR Tests
cd runtime/experiments/applications/ocr/hyde && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/ocr/handwriting && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/ocr/fivebyeight && ./run.sh && cd ../../../../..

# FIXME: Install scripts will only work after on Ubuntu 20 LTS
# cd runtime/experiments/applications/ocr/by_word && ./install.sh && cd ../../../../..
# cd runtime/experiments/applications/ocr/by_word && ./run.sh && cd ../../../../..

# cd runtime/experiments/applications/ocr/by_font && ./install.sh && cd ../../../../..
# cd runtime/experiments/applications/ocr/by_font && ./run.sh && cd ../../../../..

# cd runtime/experiments/applications/ocr/by_dpi && ./install.sh && cd ../../../../..
# cd runtime/experiments/applications/ocr/by_dpi && ./run.sh && cd ../../../../..

# EKF Build
cd runtime/tests && make -B clean tinyekf && cd ../..

# EKF Tests
cd runtime/experiments/applications/ekf/by_iteration && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/ekf/one_iteration && ./run.sh && cd ../../../../..

# cifar10 Build
cd runtime/tests && make -B clean cifar10 && cd ../..

# cifar10 Tests
cd runtime/experiments/applications/imageclassification && ./run.sh && cd ../../../..

# sod Build
cd runtime/tests && make -B clean sod && cd ../..

# sod Tests
cd runtime/experiments/applications/imageresize/test && ./install.sh && cd ../../../../..
cd runtime/experiments/applications/imageresize/test && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/imageresize/by_resolution && ./install.sh && cd ../../../../..
cd runtime/experiments/applications/imageresize/by_resolution && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/licenseplace/1 && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/licenseplace/2 && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/licenseplace/4 && ./run.sh && cd ../../../../..
cd runtime/experiments/applications/licenseplace && ./run.sh && cd ../../../..
