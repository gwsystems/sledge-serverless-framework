# SLEdgeScale

**SLEdgeScale** is an ultra-low-latency, high-density, and task-deadline-aware serverless computing solution suitable for edge environments, extending **SLEdge**. It leverages WebAssembly sandboxing provided by the [aWsm compiler](https://github.com/gwsystems/aWsm) and kernel-bypass RPC offered by [eRPC](https://github.com/erpc-io/eRPC).

## Setting up a development environment (Native on Debian Host)
SLEdgeScale was developed and tested on [Cloudlab](https://www.cloudlab.us) nodes (d6515) equipped with Mellanox NICs. A public [profile](https://www.cloudlab.us/p/GWCloudLab/sledge-rpc2) is available on CloudLab for easily creating a development environment for eRPC with node d6515. If you plan to set up the environment on machines with Intel NICs or other machines, please refer to the [eRPC](https://github.com/erpc-io/eRPC) repository for details about environment configuration, including driver and DPDK installation. For Mellanox NICs, please follow [this](https://docs.nvidia.com/networking/display/mlnxofedv590560125/installing+mlnx_ofed) guide to install *MLNX_OFED*.

For using CloudLab profile to create the development environment:
Choose the [profile](https://www.cloudlab.us/p/GWCloudLab/sledge-rpc2) and use the following configuration:
Number of Nodes: 2
Select OS image: SLEDGE
Optional physical node type : d6515

Now the environment is prepared for eRPC. The following steps are to build and install SLEdgeScale:
1. Git clone this repo and checkout branch *compare_dispatchers*
2. Extend the root filesystem:
   ```sh
   cd sledge-serverless-framework/runtime/tests
   ./add_partition.sh
   ```
4. Move `sledge-serverless-framework` to `/my_mount/`
5. Disable multiple threads:
   ```sh
   cd sledge-serverless-framework/runtime/tests
   sudo ./no_hyperthreads.sh
   ```
7. Build:
   ```sh
   cd sledge-serverless-framework
   ./install_deb.sh
   source $HOME/.cargo/env
   source ~/.bashrc
   make install
   ```
There are a set of benchmarking applications in the `/sledge/applications` directory. Run the following to compile all benchmarks runtime tests using the aWsm compiler and then copy all resulting `<application>.wasm.so` files to /sledge/runtime/bin.

```bash
cd /sledge/applications/
make clean all
```

All binary files are generated in `sledge-serverless-framework/runtime/bin`. You now have everything that you need to execute your first serverless function on SLEdgeScale

## Running your first serverless function

An SLEdgeScale serverless function consists of a shared library (\*.so) and a JSON configuration file that determines how the runtime should execute the serverless function. We first need to prepare this configuration file. As an example, here is the configuration file for our sample fibonacci function:

```json
[
        {
                "name": "gwu",
                "port": 31850,
                "replenishment-period-us": 0,
                "max-budget-us": 0,
                "routes": [
                    {
                        "route": "/fib",
                        "request-type": 1,
                        "n-resas": 1,
                        "group-id": 1,
                        "path": "fibonacci.wasm.so",
                        "admissions-percentile": 70,
                        "expected-execution-us": 5,
                        "relative-deadline-us": 50,
                        "http-resp-content-type": "text/plain"
                    }
                ]

        }

]
```

`port`:Refers to the UDP port. 
`request-type` and `path`: Used to determine which serverless function will be served; `request-type` must be unique per function.
`route`: An inherited field from SLEdge. It is not used currently but is kept to avoid parse errors.
`n-resas`: Specifies the number of CPU cores reserved for this serverless function. It is used by the DARC algorithm.
`group-id`: Specifies the group identifier used in the DARC algorithm. 
`expected-execution-us`: Currently not used. SLEdgeScale will estimate execution time online.
`relative-deadline-us`: Specifies the request deadline in microseconds.
`http-resp-content-type`: Not used currently but is kept to avoid parse errors.

### Start the SLEdgeScale Server
First, set the public IPs and ports for eRPC. Open `sledge-serverless-framework/eRPC/scripts/autorun_process_file` — the first line specifies the server IP and port, and the second line specifies the client IP and port. Make sure to apply the same change on the client machine as well.

Then we need to export some environment variables before start the server. The commonly used environment variables are:

`SLEDGE_DISABLE_PREEMPTION`: Disables the timer that sends a SIGALRM signal every 5 ms for preemption. Must disable in SLEdgeScale.

`SLEDGE_DISPATCHER`: Specifies the dispatcher policy. There are seven types of dispatchers:
- SHINJUKU: Requests are enqueued to each dispatcher's typed queue.
- EDF_INTERRUPT: The dispatcher policy used by SLEdgeScale.
- DARC: Requests are enqueued to each dispatcher's typed queue.
- LLD: The dispatcher selects the worker with the least loaded queue to enqueue a request..
- TO_GLOBAL_QUEUE: The dispatcher policy used by SLEdge. All dispatchers enqueue requests to a global queue.
- RR: The dispatcher selects a worker in a round-robin fashion.
- JSQ: The dispatcher selects the worker with the shortest queue to enqueue a request.
  
`SLEDGE_DISABLE_GET_REQUESTS_FROM_GQ`: Disable workers fetching requests from the global queue. Must be disabled if the dispatcher policy is not set to TO_GLOBAL_QUEUE.

`SLEDGE_SCHEDULER`: Specifies the scheduler policy. There are two types of schedulers:
- FIFO: First-In-First-Out. Must use the TO_GLOBAL_QUEUE dispatch policy when using FIFO.
- EDF: Earliest-deadline-first.
  
`SLEDGE_FIFO_QUEUE_BATCH_SIZE`: When using the FIFO scheduler, specifies how many requests are fetched from the global queue to the local queue each time the local queue becomes empty.

`SLEDGE_DISABLE_BUSY_LOOP`: Disables the worker’s busy loop for fetching requests from the local or global queue. The busy loop must be enabled if the dispatcher policy is set to `TO_GLOBAL_QUEUE`.

`SLEDGE_DISABLE_AUTOSCALING`: Currently not used;always set to `true`.

`SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION`: For the `hash` function, enabling this option allows SLEdgeScale to estimate the function’s execution time based on the input number. For other types of functions, this should be disabled.

`SLEDGE_FIRST_WORKER_COREID`: Specifies the ID of the first core for the worker thread. Cores 0–2 are reserved, so numbering should start from 3.

`SLEDGE_NWORKERS`: The total number of workers in the system. 

`SLEDGE_NLISTENERS`: The total number of dispachers in the system. 

`SLEDGE_WORKER_GROUP_SIZE`: The number of workers in each worker group. Its value is equal to SLEDGE_NWORKERS / SLEDGE_NLISTENERS

`SLEDGE_SANDBOX_PERF_LOG`: Server log file path

Now run the sledgert binary with the following script using sudo, passing the JSON file (e.g., the above Fibonacci function configuration) of the serverless function we want to serve. Because serverless functions are loaded by SLEdgeScale as shared libraries, we want to add the `applications/` directory to LD_LIBRARY_PATH:

```sh
#!/bin/bash

declare project_path="$(
        cd "$(dirname "$0")/../.."
        pwd
)"
echo $project_path
path=`pwd`
export SLEDGE_DISABLE_PREEMPTION=true
export SLEDGE_DISABLE_GET_REQUESTS_FROM_GQ=true
export SLEDGE_FIFO_QUEUE_BATCH_SIZE=5
export SLEDGE_DISABLE_BUSY_LOOP=true
export SLEDGE_DISABLE_AUTOSCALING=true
export SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION=true
export SLEDGE_FIRST_WORKER_COREID=3
export SLEDGE_NWORKERS=1
export SLEDGE_NLISTENERS=1
export SLEDGE_WORKER_GROUP_SIZE=1
export SLEDGE_SCHEDULER=EDF
export SLEDGE_DISPATCHER=EDF_INTERRUPT
export SLEDGE_SANDBOX_PERF_LOG=$path/server.log

cd $project_path/runtime/bin
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/fib.json
```
### Start the client to send requests
First git clone the client code:
```sh
git clone https://github.com/lyuxiaosu/eRPC.git
```
There are several client implementations under eRPC/apps, and you can also create your own customized client. In our setup, we use `openloop_client`, which is an open-loop client that sends requests following a Poisson distribution.
Edit `autorun_app_file`, `autorun_process_file`, and build:
```sh
cd eRPC
echo "openloop_client" > ./scripts/autorun_app_file
./build.sh
```
Our fibonacci function will parse a single argument from the rpc request that we send. Create the configuration file `eRPC/apps/openloop_client/conf` for `openloop_client`:
```
--test_ms 10000
--sm_verbose 0
--num_server_threads 1
--window_size 10
--req_size 5
--resp_size 32
--num_processes 2
--numa_0_ports 0
--numa_1_ports 1,3
--req_type 1
--rps 1000
--req_parameter 20
--warmup_rps 200
```
`test_ms`: Define the test duration time in milliseconds. 

`num_server_threads`: Specifies how many dispatcher threads to run on the server.

`req_size`: The size of the request package in bytes

`resp_size`: The size of the response package in bytes

`req_type`: The request type 

`req_parameter`: The parameter carried by the request. Here is the fibonacci number.


Now we have everything, let's run Fibonacci!

```sh
cd eRPC
./scripts/do.sh 1 0

```
The results is saved at `client.log`:
```
thread id, type id, latency, cpu time
0 1 64.492000 23
0 1 46.649000 23
0 1 45.806000 23
0 1 45.877000 22
0 1 45.416000 22
...
```
The first column is the thread ID, the second column is the request type, the third column is the end-to-end latency in microseconds, and the fourth column is the execution time in microseconds.

### High Density Test 
Since the High Density experiment involves a large number of RPC types, we need to modify the maximum number of RPC types supported by eRPC, as well as some parts of the SLEdgeScale code. These changes are temporary and not part of the permanent code base.
Please run:
```
./apply_patch.sh
```
in the `eRPC` directory (on both the client and server sides) and in the `runtime` directory, and then recompile `eRPC` and the `runtime`.

## Problems or Feedback?

If you encountered bugs or have feedback, please let us know in our [issue tracker.](https://github.com/gwsystems/sledge-serverless-framework/issues)
