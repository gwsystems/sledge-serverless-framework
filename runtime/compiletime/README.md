A WebAssembly module instance is statically linked with the backing functions implementing the wasm32 ABI, yielding a *.so file that SLEdge can execute. This ensures that the instance is able to aggressively inline and optimize this code.

They are broken into instruction types as on https://webassembly.github.io/spec/core/exec/instructions.html. They depend on common headers for the WebAssembly types located in the WebAssembly instance struct. These are located in runtime/include/common.

The stubs correspond to awsm/src/codegen/runtime_stubs.rs
