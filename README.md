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

## Using the Docker container to compile your serverless functions

To enter the docker environment, run:

```bash
./devenv.sh run
```

The first time you enter this environment, run the following to copy the awsmrt binary to /awsm/runtime/bin.

```bash
cd /awsm/runtime
make clean all
```

There are a set of benchmarking applications in the `/awsm/runtime/tests` directory. Run the following to compile all benchmarks runtime tests using silverfish and then copy all resulting `<application>_wasm.so` files to /awsm/runtime/bin.

```bash
cd /awsm/runtime/tests/
make clean all
```

You now have everything that you need to execute your first serverless function on aWsm

To exit the container:

```bash
exit
```

To stop the Docker container:

```bash
./devenv.sh stop
```

## Running your first serverless function

An aWsm serverless function consists of a shared library (*.so) and a JSON configuration file that determines how the runtime should execute the serverless function. As an example, here is the configuration file for our sample fibonacci function:. 

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

The `port` and `name` fields are used to determine the path where our serverless function will be served served. 

In our case, we are running the aWsm runtime on localhost, so our function is available at `http://localhost:10000/fibonacci`

Our fibonacci function will parse a single argument from the HTTP POST body that we send. The expected Content-Type  is "text/plain" and the buffer is sized to 1024 bytes for both the request and response. This is sufficient for our simple Fibonacci function, but this must be changed and sized for other functions, such as image processing.

Now that we understand roughly how the aWsm runtime interacts with serverless function, let's run Fibonacci!

From the root project directory of the host environment (not the Docker container!), navigate to the binary directory

```bash
cd runtime/bin/
```

Now run the awsmrt binary, passing the JSON file of the serverless function we want to serve. Because serverless functions are loaded by aWsm as shared libraries, we want to add the `runtime/tests/` directory to LD_LIBRARY_PATH.

```bash
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./awsmrt ../tests/test_fibonacci.json
```

While you don't see any output to the console, the runtime is running in the foreground.

Let's now invoke our serverless function to compute the 10th fibonacci number. I'll use [HTTPie](https://httpie.org/) to send a POST request with a body containing the parameter I want to pass to my serverless function. Feel free to use cURL or whatever network client you prefer!

Open a new terminal session and execute the following

```bash
echo "10" | http :10000
```

You should receive the following in response. The serverless function says that the 10th fibonacci number is 55, which seems to be correct!

```bash
HTTP/1.1 200 OK
Content-length: 3           
Content-type: text/plain                      

55
```

When done, terminal the aWsm runtime with `Ctrl+c`

## Removing the aWsm Runtime

If you are finished working with the aWsm runtime and wish to remove it, run the following command to delete our Docker build and runtime images.

```bash
./devenv.sh rma
```

And then simply delete this repository.

## Problems or Feedback?

If you encountered bugs or have feedback, please let us know in our [issue tracker.](https://github.com/phanikishoreg/awsm-Serverless-Framework/issues)
