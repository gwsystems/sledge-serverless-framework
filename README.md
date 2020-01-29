# aWsm (awesome)

**aWsm** is an efficient serverless runtime built with the `silverfish` compiler. It combines WebAssembly sandboxing with asynchronous I/O to provide a lightweight serverless solution suitable for edge computing.

## Host Dependencies

- Docker - [Installation Instructions](https://docs.docker.com/install/)
- libuv

If on Debian, you can install libuv with the following:

```bash
./devenv.sh install_libuv
```

## Setting up the environment

**Note: These steps require Docker. Make sure you've got it installed!**

We provide a Docker build environment configured with the dependencies and toolchain needed to build the aWsm runtime and serverless functions.

To setup this environment, run:

```bash
./devenv.sh setup
```

To enter the docker environment, run:

```bash
./devenv.sh run
```

## To run applications

### From within the Docker container environment

Run the following to copy the awsmrt binary to /awsm/runtime/bin.

```bash
cd /awsm/runtime
make clean all
```

There are a set of benchmarking applications in the `/awsm/runtime/tests` directory. Run the following to compile all benchmarks runtime tests using silverfish and then copy all resulting `<application>_wasm.so` files to /awsm/runtime/bin.

```bash
cd /awsm/runtime/tests/
make clean all
```

You've now built the binary and some tests. We will now execute these commands from the host

To exit the container:

```bash
exit
```

### From the host environment

You should be in the root project directory (not in the Docker container)

```bash
cd runtime/bin/
```

We can now run aWsm with one of the serverless functions we built. Let's run Fibonacci!

Because serverless functions are loaded by aWsm as shared libraries, we want to add the `runtime/tests/` directory to LD_LIBRARY_PATH.

```bash
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./awsmrt ../tests/test_fibonacci.json
```

The JSON file we pass contains a variety of configuration information:

```json
{
  "active": "yes",
  "name": "fibonacci",
  "path": "fibonacci_wasm.so",
  "port": 10000,
  "argsize": 1,
  "http-req-headers": [],
  "http-req-content-type": "text/plain",
  "http-req-size": 1024,
  "http-resp-headers": [],
  "http-resp-size": 1024,
  "http-resp-content-type": "text/plain"
}
```

Notice that it is configured to run on port 10000. The `name` field is also used to determine the path where our serverless function is served. In our case, our function is available at `http://localhost:10000/fibonacci`

Our fibonacci function expects an HTTP POST body of type "text/plain" which it can parse as an integer to figure out which Fibonacci number we want.

Let's get the 10th. Note that I'm using [HTTPie](https://httpie.org/) to send a POST request with a body containing the parameter I want to pass to my serverless function.

Note: You possibly run the awsmrt command in the foreground. If so, you should open a new terminal session.

```bash
echo "10" | http :10000
```

In my case, I received the following in response. The response is 55, which seems to be correct!

```bash
HTTP/1.1 200 OK
Content-length: 3           
Content-type: text/plain                      

55
```

## Stopping the Runtime

When you are finished, stop aWsm with

```bash
./devenv.sh stop
```
