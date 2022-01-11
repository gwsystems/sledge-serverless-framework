SHELL:=/bin/bash

.PHONY: all
all: awsm libsledge sledgert

.PHONY: clean
clean: awsm.clean libsledge.clean sledgert.clean

.PHONY: submodules
submodules:
	git submodule update --init --recursive

.PHONY: install
install: submodules all

# aWsm: the WebAssembly to LLVM bitcode compiler
awsm/target/release/awsm:
	cd awsm && cargo build --release

.PHONY: awsm
awsm: awsm/target/release/awsm

.PHONY: awsm.clean
awsm.clean:
	cd awsm && cargo clean

# libsledge: the support library linked with LLVM bitcode emitted by aWsm when building *.so modules
libsledge/dist/libsledge.a:
	make -C libsledge dist/libsledge.a

.PHONY: libsledge
libsledge: libsledge/dist/libsledge.a

.PHONY: libsledge.clean
libsledge.clean:
	make -C libsledge clean

# sledgert: the runtime that executes *.so modules
runtime/bin/sledgert:
	make -C runtime

.PHONY: sledgert
sledgert: runtime/bin/sledgert

.PHONY: sledgert.clean
sledgert.clean:
	make -C runtime clean

# SLEdge Applications
.PHONY: applications
applications:
	make -C applications all

.PHONY: applications.clean
applications.clean:
	make -C applications clean

# Tests
.PHONY: test
test:
	make -f test.mk all
