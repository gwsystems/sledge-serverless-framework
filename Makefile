SHELL:=/bin/bash
ARCH:=$(shell arch)

COMPILER=awsm
ROOT=${ROOT:-$(cd "$(dirname ${BASH_SOURCE:-$0})" && pwd)}
WASMCEPTION_URL=https://github.com/gwsystems/wasmception/releases/download/v0.2.0/wasmception-linux-x86_64-0.2.0.tar.gz

# TODO: Add ARM release build
.PHONY: build
build:
ifeq ($(ARCH),x86_64)
	cd ./awsm/wasmception && wget ${WASMCEPTION_URL} -O wasmception.tar.gz && tar xvfz wasmception.tar.gz && rm wasmception.tar.gz
endif
	test -f ./${COMPILER}/wasmception/dist/bin/clang || make -C ${COMPILER}/wasmception
	@cd ${COMPILER} && RUSTUP_TOOLCHAIN=stable cargo build --release && cd ${ROOT}

# Sanity check that the aWsm compiler built and is in our PATH
.PHONY: build-validate
build-validate:
	which awsm
	awsm --version

.PHONY: build-dev
build-dev:
	test -f ./${COMPILER}/wasmception/dist/bin/clang || make -C ${COMPILER}/wasmception
	@echo "Building aWsm compiler (default==debug)"
	@cd ${COMPILER} && cargo build && cd ${ROOT}

.PHONY: clean
clean:
	@echo "Cleaning aWsm compiler"
	@cd ${COMPILER} && cargo clean && cd ${ROOT}

# wasmception is too slow to recompile, 
# so lets not make that part of the "aWsm" cleanup
.PHONY: wclean
wclean:
	@echo "Cleaning wasmception toolchain"
	@cd ${COMPILER} && make -C wasmception clean && cd ${ROOT}

.PHONY: rtinit
rtinit:
	@echo "Building runtime for the first time!"
	make -C runtime init

.PHONY: runtime
runtime:
	@echo "Building runtime!"
	make -C runtime

.PHONY: install
install: build rtinit
	@./install.sh wasmception

.PHONY: libsledge
libsledge:
	make -C libsledge clean all
