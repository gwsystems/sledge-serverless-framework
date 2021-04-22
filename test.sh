#!/bin/bash
# Test Driver Script
# TODO: Consider refactoring into Make infrastructure

if [[ $0 != "./test.sh" ]]; then
	echo "Must run in same directory as ./test.sh"
	exit 1
fi

base_dir=$(pwd)

declare -ra tests=(
	ocr_hyde
	ocr_handwriting
	ocr_fivebyeight
	ocr_by_word
	ocr_by_font
	ocr_by_dpi
	ekf_by_iteration
	ekf_one_iteration
	image_classification
	image_resize
	lpd_by_resolution
	lpd_by_plate_count
	bimodal
	concurrency
	payload
)

declare -a failed_tests=()

# OCR Tests
# FIXME: OCR tests seem to sporadically fail and then work on rerun.
ocr_hyde() {
	# FIXME: This check is a hack because GitHub Actions is caching
	# the *.so file in the destination file, not the subodule it built from
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/hyde" || exit 1
	./run.sh || failed_tests+=("ocr_hyde")
	popd || exit 1
	return 0
}

ocr_handwriting() {
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/handwriting" || exit 1
	./run.sh || failed_tests+=("ocr_handwriting")
	popd || exit 1
	return 0
}

ocr_fivebyeight() {
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/fivebyeight" || exit 1
	./run.sh || failed_tests+=("ocr_fivebyeight")
	popd || exit 1
	return 0
}

ocr_by_word() {
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/by_word" || exit 1
	./install.sh || exit 1
	./run.sh || failed_tests+=("ocr_by_word")
	popd || exit 1
	return 0
}

ocr_by_font() {
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/by_font" || exit 1
	./install.sh || exit 1
	./run.sh || failed_tests+=("ocr_by_font")
	popd || exit 1
	return 0
}

ocr_by_dpi() {
	if [[ ! -f "$base_dir/runtime/bin/gocr_wasm.so" ]]; then
		make gocr -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ocr/by_dpi" || exit 1
	./install.sh || exit 1
	./run.sh || failed_tests+=("ocr_by_dpi")
	popd || exit 1
	return 0
}

# EKF Tests
ekf_by_iteration() {
	if [[ ! -f "$base_dir/runtime/bin/ekf_wasm.so" ]]; then
		make tinyekf -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ekf/by_iteration" || exit 1
	./run.sh || failed_tests+=("ocr_by_dpi")
	popd || exit 1
	return 0
}

ekf_one_iteration() {
	if [[ ! -f "$base_dir/runtime/bin/ekf_wasm.so" ]]; then
		make tinyekf -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/ekf/one_iteration" || exit 1
	./run.sh || failed_tests+=("ekf_one_iteration")
	popd || exit 1
	return 0
}

# cifar10 Tests
image_classification() {
	if [[ ! -f "$base_dir/runtime/bin/cifar10_wasm.so" ]]; then
		make cifar10 -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/imageclassification" || exit 1
	./run.sh || failed_tests+=("image_classification")
	popd || exit 1
	return 0
}

image_resize() {
	if [[ ! -f "$base_dir/runtime/bin/resize_wasm.so" ]]; then
		make sod -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/imageresize/test" || exit 1
	./install.sh || exit 1
	./run.sh || failed_tests+=("image_resize")
	popd || exit 1
	return 0
}

lpd_by_resolution() {
	if [[ ! -f "$base_dir/runtime/bin/lpd_wasm.so" ]]; then
		make sod -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/imageresize/by_resolution" || exit 1
	./install.sh || exit 1
	./run.sh || failed_tests+=("lpd_by_resolution")
	popd || exit 1
	return 0
}

lpd_by_plate_count() {
	if [[ ! -f "$base_dir/runtime/bin/lpd_wasm.so" ]]; then
		make sod -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/applications/licenseplate/by_plate_count" || exit 1
	./run.sh || failed_tests+=("lpd_by_plate_count")
	popd || exit 1
	return 0
}

bimodal() {
	echo "Bimodal"
	if [[ ! -f "$base_dir/runtime/bin/fibonacci_wasm.so" ]]; then
		make rttests -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/bimodal/" || exit 1
	./run.sh || failed_tests+=("bimodal")
	popd || exit 1
	return 0
}

concurrency() {
	echo "Concurrency"
	if [[ ! -f "$base_dir/runtime/bin/empty_wasm.so" ]]; then
		make rttests -C "$base_dir/runtime/tests" || exit 1
	fi
	pushd "$base_dir/runtime/experiments/concurrency/" || exit 1
	./run.sh || failed_tests+=("concurrency")
	popd || exit 1
	return 0
}

payload() {
	echo "Payload"
	if [[ ! -f "$base_dir/runtime/bin/work1k_wasm.so" ]] \
		|| [[ ! -f "$base_dir/runtime/bin/work10k_wasm.so" ]] \
		|| [[ ! -f "$base_dir/runtime/bin/work100k_wasm.so" ]] \
		|| [[ ! -f "$base_dir/runtime/bin/work1m_wasm.so" ]]; then
		make rttests -C "$base_dir/runtime/tests" || exit 1
	fi
	# TODO: Make Dependency "work1k_wasm.so" "work10k_wasm.so" "work100k_wasm.so" "work1m_wasm.so"
	pushd "$base_dir/runtime/experiments/payload/" || exit 1
	./run.sh || failed_tests+=("payload")
	popd || exit 1
	return 0
}

main() {
	cd "$base_dir/awsm" && cargo build --release || exit 1
	make all -C "$base_dir/runtime" || exit 1

	if (($# == 0)); then
		# If no arguments are provided, run all tests
		for test in "${tests[@]}"; do
			"$test"
		done

	else
		# Otherwise, only run the tests passed as arguments
		for test in "$@"; do
			if [[ ! " ${tests[*]} " =~ " ${test} " ]]; then
				printf "Error: %s is not a known test\n" "$test"
				return 1
			else
				"$test"
			fi
		done

	fi

	local -i failure_count=${#failed_tests[@]}
	if ((failure_count > 0)); then
		printf "Failed Tests\n"
		for test in "${failed_tests[@]}"; do
			printf "\t%s\n" "$test"
		done
		exit 1
	else
		printf "All tests passed\n"
		exit 0
	fi
}

main "$@"
