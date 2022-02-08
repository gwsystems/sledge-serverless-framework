SHELL:=/bin/bash

.PHONY: all
all: awsm libsledge runtime

.PHONY: clean
clean: awsm.clean libsledge.clean runtime.clean

.PHONY: submodules
submodules:
	git submodule update --init --recursive

.PHONY: install
install: submodules all

# aWsm: the WebAssembly to LLVM bitcode compiler
.PHONY: awsm
awsm: 
	cd awsm && cargo build --release

.PHONY: awsm.clean
awsm.clean:
	cd awsm && cargo clean

# libsledge: the support library linked with LLVM bitcode emitted by aWsm when building *.so modules
.PHONY: libsledge
libsledge:
	make -C libsledge dist/libsledge.a

.PHONY: libsledge.clean
libsledge.clean:
	make -C libsledge clean

# sledgert: the runtime that executes *.so modules
.PHONY: runtime
runtime:
	make -C runtime


.PHONY: runtime.clean
runtime.clean:
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
