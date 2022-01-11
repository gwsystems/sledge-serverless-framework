AWSMCC=awsm
CC=clang

# Used by aWsm when compiling the *.wasm to *.bc
AWSMFLAGS= --inline-constant-globals --runtime-globals

# Used by clang when compiling the *.so module
# FIXME: Why does -O0 cause compilation to fail?
CFLAGS=-O3 -flto

# Compiletime Components to link into *.so
SLEDGE_BASE_DIR=../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_INSTALL_TARGET=${SLEDGE_RT_DIR}/bin/

dist:
	mkdir dist

.PHONY: all
all: \
	cifar10.install \
	empty.install \
	fibonacci.install \
	gocr.install \
	gps_ekf.install \
	license_plate_detection.install \
	resize_image.install

.PHONY: clean
clean:
	@make -C wasm_apps clean
	@rm -rf dist
	@rm -rf ../../runtime/bin/*.so

wasm_apps/dist/%.wasm:
	make -C wasm_apps $(addprefix dist/,$(notdir $@))

../libsledge/dist/libsledge.a:
	make -C .. libsledge

dist/%.bc: ./wasm_apps/dist/%.wasm dist
	${AWSMCC} ${AWSMFLAGS} $< -o $@

dist/%.ll: dist/%.bc
	llvm-dis-12 $< -o $@

dist/%.wasm.so: dist/%.bc ../libsledge/dist/libsledge.a
	${CC} --shared -fPIC ${CFLAGS} $^ -o $@

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

.PHONY: gocr.install
gocr.install: ../runtime/bin/gocr.wasm.so

.PHONY: resize_image.install
resize_image.install: ../runtime/bin/resize_image.wasm.so

.PHONY: gps_ekf.install
gps_ekf.install: ../runtime/bin/gps_ekf.wasm.so

.PHONY: license_plate_detection.install
license_plate_detection.install: ../runtime/bin/license_plate_detection.wasm.so
