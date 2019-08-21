SFCC='silverfish'
ROOT=${ROOT:-$(cd "$(dirname ${BASH_SOURCE:-$0})" && pwd)}

.PHONY: build
build:
	@echo "Building wasmception toolchain, takes a while."
	@cd ${SFCC} && make -C wasmception && cd ${ROOT}
	@echo "Building silverfish compiler (release)"
	@cd ${SFCC} && cargo build --release && cd ${ROOT}

.PHONY: build-dev
build-dev:
	@echo "Building wasmception toolchain, takes a while."
	@cd ${SFCC} && make -C wasmception && cd ${ROOT}
	@echo "Building silverfish compiler (default==debug)"
	@cd ${SFCC} && cargo build && cd ${ROOT}

.PHONY: clean
clean:
	@echo "Cleaning silverfish compiler"
	@cd ${SFCC} && cargo clean && cd ${ROOT}

# wasmception is too slow to recompile, 
# so lets not make that part of the "silverfish" cleanup
.PHONY: wclean
wclean:
	@echo "Cleaning wasmception toolchain"
	@cd ${SFCC} && make -C wasmception clean && cd ${ROOT}

.PHONY: runtime
runtime:
	@echo "Building runtime!"
	make -C runtime

.PHONY: install
install: build
	@./install.sh wasmception

