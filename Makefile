SHELL:=/bin/bash

.PHONY: all
all: awsm libsledge runtime applications

.PHONY: clean
clean: awsm.clean libsledge.clean runtime.clean applications.clean

.PHONY: submodules
submodules:
	git submodule update --init --recursive

.PHONY: install
install: submodules wasm_apps all

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

# Instead of having two copies of wasm_apps, just link to the awsm repo's copy
wasm_apps:
	ln -sr awsm/applications/wasm_apps/ applications/

# Tests
.PHONY: test
test:
	make -f test.mk all
