AWSMCC=../awsm/target/release/awsm
CC=clang

# Used by aWsm when compiling the *.wasm to *.bc
AWSMFLAGS= --inline-constant-globals --runtime-globals

# Used by clang when compiling the *.so module
# --whole-archive causes the symbols in the listed static archive to be exported from the resulting *.so
# https://stackoverflow.com/questions/805555/ld-linker-question-the-whole-archive-option
CFLAGS=-O3 -flto
LDFLAGS=-shared -fPIC -Wl,--export-dynamic,--whole-archive -L../libsledge/dist/ -lsledge -Wl,--no-whole-archive
# LDFLAGS=-flto -fvisibility=hidden

# Strips out calls to assert() and disables debuglog
CFLAGS+=-DNDEBUG

dist:
	mkdir -p dist

.PHONY: all
all: \
	cifar10.install \
	empty.install \
	fibonacci.install \
	gocr.install \
	gps_ekf.install \
	license_plate_detection.install \
	resize_image.install \
	cnn_face_detection.install \
	get_jpeg_resolution.install \

.PHONY: clean
clean:
	@make -C wasm_apps clean
	@rm -rf dist
	@rm -rf ../runtime/bin/*.so

wasm_apps/dist/%.wasm:
	make -C wasm_apps $(addprefix dist/,$(notdir $@))

../libsledge/dist/:
	mkdir ../libsledge/dist

../libsledge/dist/libsledge.a: ../libsledge/dist/
	make -C .. libsledge

PHONY: scratch_storage
scratch_storage:
	make -C scratch_storage all

PHONY: scratch_storage.install
scratch_storage.install: \
	scratch_storage_get.install \
	scratch_storage_set.install \
	scratch_storage_delete.install \
	scratch_storage_upsert.install

scratch_storage/scratch_storage_%.wasm:
	make -C scratch_storage all

dist/scratch_storage_%.bc: scratch_storage/scratch_storage_%.wasm dist
	${AWSMCC} ${AWSMFLAGS} $< -o $@

dist/%.bc: ./wasm_apps/dist/%.wasm dist
	${AWSMCC} ${AWSMFLAGS} $< -o $@

dist/%.ll: dist/%.bc
	llvm-dis $< -o $@

dist/%.wasm.so: dist/%.bc
	${CC} ${CFLAGS} ${LDFLAGS} $^ -o $@

../runtime/bin/%.wasm.so: dist/%.wasm.so
	cp $^ $@

.PHONY: cifar10.install
cifar10.install: ../runtime/bin/cifar10.wasm.so

# Echo?

.PHONY: empty.install
empty.install: ../runtime/bin/empty.wasm.so

.PHONY: exit.install
exit.install: ../runtime/bin/exit.wasm.so

.PHONY: fibonacci.install
fibonacci.install: ../runtime/bin/fibonacci.wasm.so

.PHONY: asc-fib.install
asc-fib.install: ../runtime/bin/asc-fib.wasm.so

.PHONY: gocr.install
gocr.install: ../runtime/bin/gocr.wasm.so

.PHONY: resize_image.install
resize_image.install: ../runtime/bin/resize_image.wasm.so

.PHONY: gps_ekf.install
gps_ekf.install: ../runtime/bin/gps_ekf.wasm.so

.PHONY: license_plate_detection.install
license_plate_detection.install: ../runtime/bin/license_plate_detection.wasm.so

.PHONY: cnn_face_detection.install
cnn_face_detection.install: ../runtime/bin/cnn_face_detection.wasm.so

.PHONY: get_jpeg_resolution.install
get_jpeg_resolution.install: ../runtime/bin/get_jpeg_resolution.wasm.so

.PHONY: trap_divzero.install
trap_divzero.install: ../runtime/bin/trap_divzero.wasm.so

.PHONY: stack_overflow.install
stack_overflow.install: ../runtime/bin/stack_overflow.wasm.so

.PHONY: html.install
html.install: ../runtime/bin/html.wasm.so

.PHONY: scratch_storage_get.install
scratch_storage_get.install: ../runtime/bin/scratch_storage_get.wasm.so

.PHONY: scratch_storage_set.install
scratch_storage_set.install: ../runtime/bin/scratch_storage_set.wasm.so

.PHONY: scratch_storage_delete.install
scratch_storage_delete.install: ../runtime/bin/scratch_storage_delete.wasm.so

.PHONY: scratch_storage_upsert.install
scratch_storage_upsert.install: ../runtime/bin/scratch_storage_upsert.wasm.so

.PHONY: depth_to_xyz.install
depth_to_xyz.install: ../runtime/bin/depth_to_xyz.wasm.so
