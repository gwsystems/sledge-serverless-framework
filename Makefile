COMPILER='awsm'
ROOT=${ROOT:-$(cd "$(dirname ${BASH_SOURCE:-$0})" && pwd)}

.PHONY: build
build:
	@echo "Building wasmception toolchain, takes a while."
	@cd ${COMPILER} && make -C wasmception && cd ${ROOT}
	@echo "Building aWsm compiler (release)"
	@cd ${COMPILER} && cargo build --release && cd ${ROOT}

.PHONY: build-dev
build-dev:
	@echo "Building wasmception toolchain, takes a while."
	@cd ${COMPILER} && make -C wasmception && cd ${ROOT}
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

