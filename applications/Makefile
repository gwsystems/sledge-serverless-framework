AWSMCC=awsm
CC=clang

OPTFLAGS=-O3 -flto

# Compiletime Components to link into *.so
SLEDGE_BASE_DIR=../
SLEDGE_RT_DIR=${SLEDGE_BASE_DIR}/runtime/
SLEDGE_INSTALL_TARGET=${SLEDGE_RT_DIR}/bin/

dist:
	mkdir dist

.PHONY: all
all: fibonacci.install empty.install ekf.install cifar10.install gocr.install lpd.install resize.install

.PHONY: clean
clean:
	@make -C wasm_apps clean
	@rm -rf dist
	@rm -rf ../../runtime/bin/*.so

wasm_apps/dist/%.wasm:
	make -C wasm_apps all

../libsledge/dist/libsledge.a:
	make -C .. dist/libsledge.a

dist/%.bc: ./wasm_apps/dist/%.wasm dist
	${AWSMCC} --inline-constant-globals --runtime-globals $< -o $@

dist/%.wasm.so: dist/%.bc ../libsledge/dist/libsledge.a
	${CC} --shared -fPIC ${OPTFLAGS} $^ -o $@

../runtime/bin/%.wasm.so: dist/%.wasm.so
	cp $^ $@

.PHONY: fibonacci.install
fibonacci.install: ../runtime/bin/fibonacci.wasm.so

.PHONY: empty.install
empty.install: ../runtime/bin/empty.wasm.so

.PHONY: exit.install
exit.install: ../runtime/bin/exit.wasm.so

# EKF
dist/ekf.wasm.so: dist/gps_ekf.wasm.so
	mv $< $@

.PHONY: ekf.install
ekf.install: ../runtime/bin/ekf.wasm.so

# CIFAR10
.PHONY: cifar10.install
cifar10.install: ../runtime/bin/cifar10.wasm.so

# GOCR
.PHONY: gocr.install
gocr.install: ../runtime/bin/gocr.wasm.so

# LPD
dist/lpd.wasm.so: dist/license_plate_detection.wasm.so
	mv $< $@

.PHONY: lpd.install
lpd.install: ../runtime/bin/lpd.wasm.so

# Resize
dist/resize.wasm.so: dist/resize_image.wasm.so
	mv $< $@

.PHONY: resize.install
resize.install: ../runtime/bin/resize.wasm.so
