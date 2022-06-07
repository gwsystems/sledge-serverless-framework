include ../wasm_apps/common.mk

.PHONY: all
all: \
	scratch_storage_get.wasm \
	scratch_storage_set.wasm \
	scratch_storage_delete.wasm \
	scratch_storage_upsert.wasm \

.PHONY: clean
clean:
	@rm -f scratch_storage_set.wa* scratch_storage_get.wa* scratch_storage_delete.wa* scratch_storage_upsert.wa*

scratch_storage_set.wasm: scratch_storage_set.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

scratch_storage_get.wasm: scratch_storage_get.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

scratch_storage_delete.wasm: scratch_storage_delete.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

scratch_storage_upsert.wasm: scratch_storage_upsert.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@
