---
geometry: margin=2cm
---

# libsledge Binary Interfaces

libsledge is a \*.a static library (archive) that is statically linked with a \*.bc file generated by the aWsm compiler when compiling to a \*.so Linux shared library that can be loaded and executed by the sledgert runtime.

The static library internally implements the aWsm ABI in order to link to the \*.bc file generated by the aWsm compiler. [See the relevant documentation for this ABI here](../awsm/doc/abi.md).

libsledge defines a ABI between the sledgert runtime and a \*.so shared library containing an executable serverless function. This is distinct from the aWsm ABI.

# SLEdge \*.so serverless module

A SLEdge \*.so serverless module is generated by the latter portion of the aWsm/SLEdge toolchain.

The first portion of the toolchain is responsible for compiling a source program into a WebAssembly module. This is handled by standard compilers capable of emitting WebAssembly.
The second portion of the toolchain is the aWsm compiler, which generates a \*.bc file with a well defined ABI.
The third portion of the toolchain is the LLVM compiler, which ingests a \*.bc file emitted by aWsm and the libsledge static library, and emits a SLEdge \*.so serverless module.

## Architecture

In order to reduce the overhead of calling sledgert functions, libsledge operates on global state of type `sledge_abi__wasm_module_instance` at `sledge_abi__current_wasm_module_instance`. This represents the global state of the wasm32 context executing on a sledgert worker core. The scheduler is responsible for populating these symbols before yielding execution to a serverless function.

The `sledge_abi__wasm_module_instance` structure includes the WebAssembly function table and the WebAssembly linear memory. This subset was selected because the author believes that use of function pointers and linear memory is frequent enough that LTO when compiling the \*.so file is beneficial.

## WebAssembly Instruction Implementation

Here is a list of WebAssembly instructions that depend on symbols from libsledge, libc, or sledgert (via the SLEdge ABI).

### [Control Instructions](https://webassembly.github.io/spec/core/syntax/instructions.html#control-instructions)

| Instruction   | aWsm ABI                  | libc Dependencies   | SLEdge ABI                                                                                                                              |
| ------------- | ------------------------- | ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| call_indirect | `get_function_from_table` | `stderr`, `fprintf` | `sledge_abi__current_wasm_module_instance.table`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_INVALID_INDEX`, `WASM_TRAP_MISMATCHED_TYPE` |

### [Variable Instructions](https://webassembly.github.io/spec/core/syntax/instructions.html#variable-instructions)

| Instruction | aWsm ABI                           | libc Dependencies | SLEdge ABI                                                             |
| ----------- | ---------------------------------- | ----------------- | ---------------------------------------------------------------------- |
| global.get  | `get_global_i32`, `get_global_i64` | None              | `sledge_abi__wasm_globals_get_i32`, `sledge_abi__wasm_globals_get_i64` |
| global.set  | `set_global_i32`, `set_global_i64` | None              | `sledge_abi__wasm_globals_set_i32`, `sledge_abi__wasm_globals_set_i64` |

### [Numeric Instructions](https://webassembly.github.io/spec/core/syntax/instructions.html#numeric-instructions)

| Instruction     | aWsm ABI                                 | libc Dependencies                             | SLEdge ABI                                                              |
| --------------- | ---------------------------------------- | --------------------------------------------- | ----------------------------------------------------------------------- |
| i32.div_s       | `i32_div` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.div_u       | `u32_div` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.rem_s       | `i32_rem` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.rem_u       | `u32_rem` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.rotl        | `rotl_u32`                               | None                                          | None                                                                    |
| i32.rotr        | `rotr_u32`                               | None                                          | None                                                                    |
| i32.trunc_f32_s | `i32_trunc_f32` ("fast unsafe" disabled) | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.trunc_f32_u | `u32_trunc_f32` ("fast unsafe" disabled) | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.trunc_f64_s | `i32_trunc_f64` ("fast unsafe" disabled) | `INT32_MIN`, `INT32_MAX`, `stderr`, `fprintf` | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i32.trunc_f64_u | `u32_trunc_f64` ("fast unsafe" disabled) | `UINT32_MAX`, `stderr`, `fprintf`             | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.div_s       | `i64_div` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.div_u       | `u64_div` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.rem_s       | `i64_rem` ("fast unsafe" disabled)       | `INT32_MIN`, `stderr`, `fprintf`              | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.rem_u       | `u64_rem` ("fast unsafe" disabled)       | `stderr`, `fprintf`                           | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.rotl        | `rotl_u64`                               | **NOT SUPPORTED**                             | **NOT SUPPORTED**                                                       |
| i64.rotr        | `rotr_u64`                               | **NOT SUPPORTED**                             | **NOT SUPPORTED**                                                       |
| i64.trunc_f32_s | `i64_trunc_f32`                          | `INT64_MIN`, `INT64_MAX`, `stderr`, `fprintf` | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.trunc_f32_u | `u64_trunc_f32`                          | `UINT64_MAX`, `stderr`, `fprintf`             | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.trunc_f64_s | `i64_trunc_f64`                          | `INT64_MIN`, `INT64_MAX`, `stderr`, `fprintf` | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| i64.trunc_f64_u | `u64_trunc_f64`                          | `UINT64_MAX`, `stderr`, `fprintf`             | `sledge_abi__wasm_trap_raise`, `WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION` |
| f32.ceil        | `f32_ceil`                               | `ceilf`                                       | `ceilf`                                                                 |
| f32.copysign    | `f32_copysign`                           | `copysignf`                                   | `copysignf`                                                             |
| f32.floor       | `f32_floor`                              | `floorf`                                      | `floorf`                                                                |
| f32.max         | `f32_max`                                | None                                          | None                                                                    |
| f32.min         | `f32_min`                                | None                                          | None                                                                    |
| f32.nearest     | `f32_nearest`                            | `nearbyintf`                                  | `nearbyintf`                                                            |
| f32.trunc       | `f32_trunc_f32`                          | `truncf`                                      | `truncf`                                                                |
| f64.ceil        | `f64_ceil`                               | `ceil`                                        | `ceil`                                                                  |
| f64.copysign    | `f64_copysign`                           | `copysign`                                    | `copysign`                                                              |
| f64.floor       | `f64_floor`                              | `floor`                                       | `floor`                                                                 |
| f64.max         | `f64_max`                                | None                                          | None                                                                    |
| f64.min         | `f64_min`                                | None                                          | None                                                                    |
| f64.nearest     | `f64_nearest`                            | `nearbyint`                                   | `nearbyint`                                                             |
| f64.trunc       | `f64_trunc_f64`                          | `trunc`                                       | `trunc`                                                                 |

### [Memory Instructions](https://webassembly.github.io/spec/core/syntax/instructions.html#memory-instructions)

| instruction  | aWsm ABI                  | libc Dependencies   | SLEdge ABI                                                                                                                |
| ------------ | ------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| i32.load     | `get_i32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.load8_s  | `get_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.load8_u  | `get_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.load16_s | `get_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.load16_u | `get_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.store    | `set_i32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.store8   | `set_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i32.store16  | `set_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load     | `get_i64`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load8_s  | `get_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load8_u  | `get_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load16_s | `get_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load16_u | `get_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load32_s | `get_i32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.load32_u | `get_i32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.store    | `set_i64`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.store8   | `set_i8`                  | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.store16  | `set_i16`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| i64.store32  | `set_i32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| f32.load     | `get_f32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| f32.store    | `set_f32`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| f64.load     | `get_f64`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| f64.store    | `set_f64`                 | `fprintf`, `stderr` | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_trap_raise`, `WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY` |
| memory.grow  | `instruction_memory_grow` |                     | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_memory_expand`                                       |
| memory.size  | `instruction_memory_size` |                     | `sledge_abi__current_wasm_module_instance.memory`                                                                         |
| None         | `initialize_region`       |                     | `sledge_abi__current_wasm_module_instance.memory`, `sledge_abi__wasm_memory_initialize_region`                            |


# SLEdge \*.so Module Loading / Initialization

The `sledgert` runtime is invoked with an argument containing the path to a JSON file defining serverless functions. The JSON format is a top level array containing 0..N JSON objects with the following keys:
"name" - A Human friendly name identifying a serverless function. Is required.
"path" - A path to a \*.so module containing the program to be executed
"port" - The port which the serverless function is registered
"relative-deadline-us"
"expected-execution-us"
"admissions-percentile"
"http-resp-content-type"

The path to the JSON file is passed to `module_alloc_from_json`, which uses the Jasmine library to parse the JSON, performs validation, and passes the resulting specification to `module_alloc` for each module definition found. `module_alloc` allocated heap memory for a `struct module` and then calls `module_init`. `module_init` calls `sledge_abi_symbols_init`, which calls `dlopen` on the _.so file at the path specified in the JSON and then calls `dlsym` to resolve symbols within the _.so module.

- `module.abi.initialize_globals` -> `SLEDGE_ABI__INITIALIZE_GLOBALS` -> `populate_globals`
- `module.abi.initialize_memory`-> `SLEDGE_ABI__INITIALIZE_MEMORY` -> `populate_memory`
- `module.abi.initialize_table` -> `SLEDGE_ABI__INITIALIZE_TABLE` -> `populate_table`
- `module.abi.entrypoint` -> `SLEDGE_ABI__ENTRYPOINT` -> `wasmf__start`
- `module.abi.starting_pages` -> `SLEDGE_ABI__STARTING_PAGES` -> `starting_pages`
- `module.abi.max_pages` -> `SLEDGE_ABI__MAX_PAGES` -> `max_pages`
- `module.abi.globals_len` -> `SLEDGE_ABI__GLOBALS_LEN` -> `globals_len`

`module_init` then calls `module.abi.initialize_table`, which populates the indirect function table with the actual functions. This is performed once during module initialization because this table does not actually vary between instances of a module.

# SLEdge \*.so Module Instantiation

When `sledgert` receives a request at the registered port specified in the JSON, it performs allocation and initialization steps. The scheduler sets the expected ABI symbols and yields to `current_sandbox_start`, which immediately calls `current_sandbox_init`. This function initializes the associated runtime state and

1. calls `module.abi.initialize_globals` for the current sandbox if not NULL. This is optional because the module might not have been built with the `--runtime-globals`, in which case runtime globals are not used at all. If not NULL, the globals are set in the table.
2. calls `module.abi.initialize_memory`, which copies segments into the linear memory

`current_sandbox_init` calls `wasi_context_init` to initialize the WASI context within the runtime.

`current_sandbox_init` returns to `current_sandbox_start`, which sets up wasm traps using `setjmp` and then calls `module.abi.entrypoint`

# Discussion (follow-up with Github issues):

- Should `sledge_abi__current_wasm_module_instance` be turned into a macro defined int the ABI header? That way it'll be easier to change the ABI symbols (change once, applied everywhere).
- Should `instruction_memory_grow` be moved into sledgert? This would simplify the handling of the "cache" and generating a memory profile?
- Rename `sledge_abi__wasm_globals_*` to `sledge_abi__wasm_global_*`
- Implement Unsupported Numeric Instructions
- Should the wasm global table be accessed directly instead of via a runtime function? If we expose the wasm global table to libsledge, then we have worse ABI stability, but better performance.
- Should the Function Table be handled by the \*.so file or sledgert? Are function pointers really called that frequently?
