# aWsm (awesome) 

This project is a work-in-progress to build an efficient WASM runtime, **aWsm**, using `silverfish` compiler.

## Setting up the environment

To use a Docker container based environment, that makes your life easy by installing all the required dependencies and builds the toolchain for you.
Run
```
./devenv.sh setup
```
**make sure you've docker installed.**

To enter the docker environment,
```
./devenv.sh run
```
**spawns a shell in the container.**

To setup toolchain path (within a container, per `run`)
```
source /opt/awsm/bin/devenv_src.sh
```

## To run applications

There are a set of benchmarking applications in `code_benches` directory that should be "loadable", WIP!!
**All the remaining steps are in a Docker container environment.**

```
cd /awsm/tests/

make clean all
```
This compiles all benchmarks in silverfish and other runtime tests and copies `<application>_wasm.so` to /awsm/runtime/bin.

```
cd /awsm/runtime
make clean all
```
This will copy the awsmrt binary to /awsm/runtime/bin.

```
cd /awsm/runtime/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`
```

**Create input and test files**
Supports module registration using json format now and invocations as well.
More importantly, each module runs a udp server and waits for connections.
The udpclient tool in `runtime/tools` directory uses a format `<ip:port>$<json_request_string>`, 
connects to <ip:port> and sends the <json_reqest_string> to the IP address it connects at the start.

To run `awsm runtime`,
```
./awsmrt ../tests/test_modules.json
```

To run the udpclient,
```
./udpclient ../tests/test_sandboxes.jsondata
```
And follow the prompts in udpclient to send requests to the runtime.

## WIP (Work In Progress)

* ~~Dynamic loading of multiple modules~~
* ~~Multiple sandboxes (includes multiple modules, multiple instances of a module)~~
* ~~Bookkeeping of multiple modules and multiple sandboxes.~~
* ~~Runtime to "poll"?? on requests to instantiate a module~~ and respond with the result.
* ~~Runtime to schedule multiple sandboxes.~~
* Efficient scheduling and performance optimizations.
* ~~Runtime to enable event-based I/O (using `libuv`).~~ (basic I/O works with libuv)
* To enable WASI interface, perhaps through the use of WASI-SDK

## Silverfish compiler

Silverfish compiler uses `llvm` and interposes on loads/stores to enable sandbox isolation necessary in `aWsm` multi-sandboxing runtime.
`aWsm` runtime includes the compiler-runtime API required for bounds checking in sandboxes.
Most of the sandboxing isolation is copied from the silverfish runtime.
