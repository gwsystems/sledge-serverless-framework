# This Makefile builds a variety of synthetic and real-world applications ported to SLEdge
#
# Some of the tests generate experimental results in the form of charts of *.csv files. Refer to
# the relevant directory for specifics.

# OCR - Optical Character Recognition
./runtime/bin/gocr_wasm.so: 
	make gocr.install -C ./runtime/tests

PHONY: ocr__by_dpi
ocr__by_dpi: ./runtime/bin/gocr_wasm.so
	# cd ./runtime/experiments/applications/ocr/by_dpi && ./install.sh
	cd ./runtime/experiments/applications/ocr/by_dpi && ./run.sh

PHONY: ocr__by_font
ocr__by_font: ./runtime/bin/gocr_wasm.so
	cd ./runtime/experiments/applications/ocr/by_font && ./run.sh

PHONY: ocr__by_word
ocr__by_word: ./runtime/bin/gocr_wasm.so
	cd ./runtime/experiments/applications/ocr/by_word && ./run.sh

PHONY: ocr__fivebyeight
ocr__fivebyeight: ./runtime/bin/gocr_wasm.so
	cd ./runtime/experiments/applications/ocr/fivebyeight && ./run.sh

PHONY: ocr__handwriting
ocr__handwriting: ./runtime/bin/gocr_wasm.so
	cd ./runtime/experiments/applications/ocr/handwriting && ./run.sh

PHONY: ocr__hyde
ocr__hyde: ./runtime/bin/gocr_wasm.so
	cd ./runtime/experiments/applications/ocr/hyde && ./run.sh

PHONY: ocr_all
ocr__all: \
	ocr__by_dpi \
	ocr__by_font \
	ocr__by_word \
	ocr__fivebyeight \
	ocr__handwriting \
	ocr__hyde

# Extended Kalman Filter applied to binary GPS data
./runtime/bin/ekf_wasm.so:
	make ekf.install -C ./runtime/tests

PHONY: ekf__by_iteration
ekf__by_iteration: ./runtime/bin/ekf_wasm.so
	cd ./runtime/experiments/applications/ekf/by_iteration && ./run.sh 

PHONY: ekf__one_iteration
ekf__one_iteration: ./runtime/bin/ekf_wasm.so
	cd ./runtime/experiments/applications/ekf/one_iteration && ./run.sh

PHONY: ekf__all
ekf__all: \
	ekf__by_iteration \
	ekf__one_iteration

# CIFAR10-based Image Classification
./runtime/bin/cifar10_wasm.so: 
	make cifar10.install -C ./runtime/tests

PHONY: cifar10__image_classification
cifar10__image_classification: ./runtime/bin/cifar10_wasm.so
	cd ./runtime/experiments/applications/imageclassification && ./run.sh

PHONY: cifar10__all
cifar10__all: \
	cifar10__image_classification

# SOD Computer Vision / ML Applications
# https://sod.pixlab.io/

# SOD - Image Resize
./runtime/bin/resize_wasm.so:
	make resize.install -C ./runtime/tests
	
# Commented out command installs imagemagick. Requires password for sudo to install
PHONY: sod__image_resize__test
sod__image_resize__test: ./runtime/bin/resize_wasm.so
	# cd ./runtime/experiments/applications/imageresize/test && ./install.sh
	cd ./runtime/experiments/applications/imageresize/test && ./run.sh

PHONY: sod__image_resize__by_resolution
sod__image_resize__by_resolution: ./runtime/bin/resize_wasm.so
	# cd ./runtime/experiments/applications/imageresize/by_resolution && ./install.sh
	cd ./runtime/experiments/applications/imageresize/by_resolution && ./run.sh

# SOD - License Plate Detection
./runtime/bin/lpd_wasm.so:
	make lpd.install -C ./runtime/tests

PHONY: sod__lpd__by_plate_count
sod__lpd__by_plate_count: ./runtime/bin/lpd_wasm.so
	cd ./runtime/experiments/applications/licenseplate/by_plate_count && ./run.sh

PHONY: sod__all
sod__all: sod__image_resize__test sod__image_resize__by_resolution sod__lpd__by_plate_count

# Scheduler Experiments with synthetic workloads
./runtime/bin/fibonacci_wasm.so:
	make fibonacci.install -C ./runtime/tests

PHONY: fibonacci__bimodal
fibonacci__bimodal: ./runtime/bin/fibonacci_wasm.so
	cd ./runtime/experiments/bimodal/ && ./run.sh

./runtime/bin/empty_wasm.so:
	make empty.install -C ./runtime/tests

PHONY: empty__concurrency
empty__concurrency: ./runtime/bin/empty_wasm.so
	# ./runtime/experiments/concurrency/ && install.sh
	./runtime/experiments/concurrency/ && run.sh

# TODO: Refactor payload experiment

all: \
	ocr__all \
	ekf__all \
	cifar10__all \
	sod__all \
	fibonacci__bimodal \
	empty__concurrency 
