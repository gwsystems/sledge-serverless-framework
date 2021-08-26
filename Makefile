SHELL:=/bin/bash

awsm/target/release/awsm:
	@echo "Building aWsm compiler"
	@cd awsm && cargo build --release

.PHONY: awsm
awsm: awsm/target/release/awsm

.PHONY: clean-compiler
clean-compiler:
	@echo "Cleaning aWsm compiler"
	@cd awsm && cargo clean

.PHONY: clean-runtime
clean-runtime:
	@echo "Cleaning SLEdge runtime"
	@make -C runtime clean

.PHONY: clean
clean: clean-compiler clean-runtime

.PHONY: rtinit
rtinit: awsm
	@echo "Building runtime for the first time!"
	make -C runtime init

.PHONY: runtime
runtime:
	@echo "Building runtime!"
	make -C runtime

.PHONY: install
install: rtinit

