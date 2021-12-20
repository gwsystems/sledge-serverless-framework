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

PHONY: ocr__
ocr__: \
	ocr__by_dpi \
	ocr__by_font \
	ocr__by_word \
	ocr__fivebyeight \
	ocr__handwriting \
	ocr__hyde

./runtime/bin/ekf_wasm.so:
	make gps_ekf.install -C ./runtime/Tests

PHONY: ekf__by_iteration
ekf__by_iteration: ./runtime/bin/ekf_wasm.so
	cd ./runtime/experiments/applications/ekf/by_iteration && ./run.sh 

PHONY: ekf__one_iteration
ekf__one_iteration: ./runtime/bin/ekf_wasm.so
	cd ./runtime/experiments/applications/ekf/one_iteration && ./run.sh

PHONY: ekf__
ekf__: \
	ekf__by_iteration \
	ekf__one_iteration

./runtime/bin/cifar10_wasm.so: 
	make cifar10.install -C ./runtime/tests

PHONY: cifar10__image_classification
cifar10__image_classification: ./runtime/bin/cifar10_wasm.so
	cd ./runtime/experiments/applications/imageclassification && ./run.sh

./runtime/bin/resize_wasm.so:
	make resize_image.install -C ./runtime/tests

PHONY: sod__image_resize
sod__image_resize: ./runtime/bin/resize_wasm.so
	# cd ./runtime/experiments/applications/imageresize/test && ./install.sh
	cd ./runtime/experiments/applications/imageresize/test && ./run.sh

PHONY: sod__image_resize__by_resolution
sod__image_resize_by_resolution: ./runtime/bin/resize_wasm.so
	# cd ./runtime/experiments/applications/imageresize/test && ./install.sh
	cd ./runtime/experiments/applications/imageresize/by_resolution/test && ./run.sh

./runtime/bin/lpd_wasm.so:
	make license_plate_detection.install -C ./runtime/tests

PHONY: sod__lpd__by_plate_count
sod__lpd_by_plate_count: ./runtime/bin/lpd_wasm.so
	cd ./runtime/experiments/applications/licenseplate/by_plate_count ** run.sh

PHONY: sod__
sod__: sod__image_resize sod__image_resize__by_resolution sod__lpd__by_plate_count

./runtime/bin/fibonacci_wasm.so:
	make fibonacci.install -C ./runtime/tests

PHONY: fibonacci__bimodal
fibonacci__bimodal: ./runtime/bin/fibonacci_wasm.so
	cd ./runtime/experiments/bimodal/ && ./run.sh

./runtime/bin/empty_wasm.so:
	make empty.install -C ./runtime/tests

empty__concurrency: ./runtime/bin/empty_wasm.so
	# ./runtime/experiments/concurrency/ && install.sh
	./runtime/experiments/concurrency/ && run.sh

# TODO: Refactor payload experiment

all: \
	cifar10__image_classification \
	ekf__ \
	empty__concurrency \
	fibonacci__bimodal \
	ocr__ \
	sod__
