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

An SLEdgeScale serverless function consists of a shared library (\*.so) and a JSON configuration file that determines how the runtime should execute the serverless function. As an example, here is the configuration file for our sample fibonacci function:

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
`expected-execution-us`: Specifies the function’s expected execution time in microseconds. This is used for testing; SLEdgeScale also estimates execution time online.
`relative-deadline-us`: Specifies the request deadline in microseconds.
`http-resp-content-type`: Not used currently but is kept to avoid parse errors.

### Start the SLEdgeScale Server
We need to export some environment variables before start the server. The commonly used environment variables are:
`SLEDGE_DISABLE_PREEMPTION`: Disables the timer that sends a SIGALRM signal every 5 ms for preemption. Must disable in SLEdgeScale.
`SLEDGE_DISABLE_GET_REQUESTS_FROM_GQ`: Disable workers fetching requests from the global queue. Disabled in SLEdgeScale; optional in SLEdge. 
#only works for FIFO scheduler
export SLEDGE_DISABLE_GET_REQUESTS_FROM_GQ=$disable_get_req_from_GQ
export SLEDGE_FIFO_QUEUE_BATCH_SIZE=5
export SLEDGE_DISABLE_BUSY_LOOP=$disable_busy_loop
export SLEDGE_DISABLE_AUTOSCALING=$disable_autoscaling
#export SLEDGE_SIGALRM_HANDLER=TRIAGED
export SLEDGE_DISABLE_EXPONENTIAL_SERVICE_TIME_SIMULATION=$disable_service_ts_simulation
export SLEDGE_FIRST_WORKER_COREID=$first_worker_core_id
export SLEDGE_NWORKERS=$worker_num
export SLEDGE_NLISTENERS=$listener_num
export SLEDGE_WORKER_GROUP_SIZE=$worker_group_size
export SLEDGE_SCHEDULER=$scheduler_policy
#export SLEDGE_DISPATCHER=DARC
export SLEDGE_DISPATCHER=$dispatcher_policy
export SLEDGE_SCHEDULER=$scheduler_policy
#export SLEDGE_DISPATCHER=EDF_INTERRUPT
export SLEDGE_SANDBOX_PERF_LOG=$path/$server_log

Now run the sledgert binary, passing the JSON file of the serverless function we want to serve. Because serverless functions are loaded by SLEdge as shared libraries, we want to add the `applications/` directory to LD_LIBRARY_PATH.

```bash
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../../tests/fibonacci/bimodal/spec.json
In our case, we are running the SLEdge runtime on localhost, so our function is available at `localhost:10010/fib`.

Our fibonacci function will parse a single argument from the HTTP POST body that we send. The expected Content-Type is "text/plain".

Now that we understand roughly how the SLEdge runtime interacts with serverless function, let's run Fibonacci!

The fastest way to check it out is just to click on the following URL on your Web browser: [http://localhost:10010/fib?10](http://localhost:10010/fib?10)

From the root project directory of the host environment (not the Docker container!), navigate to the binary directory

```bash
cd runtime/bin/
```


```

While you don't see any output to the console, the runtime is running in the foreground.

Let's now invoke our serverless function to compute the 10th fibonacci number. We'll use `cURL` and [HTTPie](https://httpie.org/) to send a HTTP  GET and POST requests with the parameter we want to pass to my serverless function. Feel free to use whatever other network client you prefer! 

Open a **new** terminal session and execute the following

```bash
# HTTP GET method:
http localhost:10010/fib?10
curl localhost:10010/fib?10

# HTTP POST method:
echo "10" | http POST localhost:10010/fib
curl -i -d 10 localhost:10010/fib
```

You should receive the following in response. The serverless function says that the 10th fibonacci number is 55, which seems to be correct!

```bash
HTTP/1.1 200 OK
Server: SLEdge
Connection: close
Content-Type: text/plain
Content-Length: 3

55
```

When done, terminal the SLEdge runtime with `Ctrl+c`

## Running Test Workloads

Various synthetic and real-world tests can be found in `runtime/tests`. Generally, each experiment can be run by Make rules in the top level `test.mk`.

`make -f test.mk all`

## Problems or Feedback?

If you encountered bugs or have feedback, please let us know in our [issue tracker.](https://github.com/gwsystems/sledge-serverless-framework/issues)
