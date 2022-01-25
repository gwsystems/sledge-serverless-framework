# This Makefile builds a variety of synthetic and real-world applications ported to SLEdge
#
# Some of the tests generate experimental results in the form of charts of *.csv files. Refer to
# the relevant directory for specifics.

# gocr - GNU Optical Character Recognition
./runtime/bin/gocr.wasm.so: 
	make gocr.install -C ./applications

PHONY: gocr__by_dpi
gocr__by_dpi: ./runtime/bin/gocr.wasm.so
	# cd ./tests/gocr/by_dpi && ./install.sh
	cd ./tests/gocr/by_dpi  && ./run.sh

PHONY: gocr__by_font
gocr__by_font: ./runtime/bin/gocr.wasm.so
	cd ./tests/gocr/by_font && ./run.sh

PHONY: gocr__by_word
gocr__by_word: ./runtime/bin/gocr.wasm.so
	cd ./tests/gocr/by_word && ./run.sh

PHONY: gocr__fivebyeight
gocr__fivebyeight: ./runtime/bin/gocr.wasm.so
	cd ./tests/gocr/fivebyeight && ./run.sh

PHONY: gocr__handwriting
gocr__handwriting: ./runtime/bin/gocr.wasm.so
	cd ./tests/gocr/handwriting && ./run.sh

PHONY: gocr__hyde
gocr__hyde: ./runtime/bin/gocr.wasm.so
	cd ./tests/gocr/hyde && ./run.sh

PHONY: gocr_all
gocr__all: \
	gocr__by_dpi \
	gocr__by_font \
	gocr__by_word \
	gocr__fivebyeight \
	gocr__handwriting \
	gocr__hyde

# Extended Kalman Filter applied to binary GPS data
./runtime/bin/gps_ekf.wasm.so:
	make gps_ekf.install -C ./applications

PHONY: ekf__by_iteration
ekf__by_iteration: ./runtime/bin/gps_ekf.wasm.so
	cd ./tests/TinyEKF/by_iteration && ./run.sh 

PHONY: ekf__one_iteration
ekf__one_iteration: ./runtime/bin/gps_ekf.wasm.so
	cd ./tests/TinyEKF/one_iteration && ./run.sh

PHONY: ekf__all
ekf__all: \
	ekf__by_iteration \
	ekf__one_iteration

# CIFAR10-based Image Classification
./runtime/bin/cifar10.wasm.so: 
	make cifar10.install -C ./applications

PHONY: cifar10__image_classification
cifar10__image_classification: ./runtime/bin/cifar10.wasm.so
	cd ./tests/CMSIS_5_NN/imageclassification && ./run.sh

PHONY: cifar10__all
cifar10__all: \
	cifar10__image_classification

# SOD Computer Vision / ML Applications
# https://sod.pixlab.io/

# SOD - Image Resize
./runtime/bin/resize_image.wasm.so:
	make resize_image.install -C ./applications
	
# Commented out command installs imagemagick. Requires password for sudo to install
PHONY: sod__image_resize__test
sod__image_resize__test: ./runtime/bin/resize_image.wasm.so
	# cd ./tests/sod/image_resize/test && ./install.sh
	cd ./tests/sod/image_resize/test && ./run.sh

PHONY: sod__image_resize__by_resolution
sod__image_resize__by_resolution: ./runtime/bin/resize_image.wasm.so
	# cd ./tests/sod/image_resize/by_resolution && ./install.sh
	cd ./tests/sod/image_resize/by_resolution && ./run.sh

# SOD - License Plate Detection
./runtime/bin/license_plate_detection.wasm.so:
	make license_plate_detection.install -C ./applications

PHONY: sod__lpd__by_plate_count
sod__lpd__by_plate_count: ./runtime/bin/license_plate_detection.wasm.so
	cd ./tests/sod/lpd/by_plate_count && ./run.sh

PHONY: sod__all
sod__all: sod__image_resize__test sod__image_resize__by_resolution sod__lpd__by_plate_count

# Scheduler Experiments with synthetic workloads
./runtime/bin/fibonacci.wasm.so:
	make fibonacci.install -C ./applications

PHONY: fibonacci__bimodal
fibonacci__bimodal: ./runtime/bin/fibonacci.wasm.so
	cd ./tests/fibonacci/bimodal/ && ./run.sh

./runtime/bin/empty.wasm.so:
	make empty.install -C ./applications

PHONY: empty__concurrency
empty__concurrency: ./runtime/bin/empty.wasm.so
	# ./tests/empty/concurrency/ && install.sh
	./tests/empty/concurrency/ && run.sh

all: \
	gocr__all \
	ekf__all \
	cifar10__all \
	sod__all \
	fibonacci__bimodal \
	empty__concurrency 
