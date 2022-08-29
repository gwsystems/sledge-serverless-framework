# SLEdge

**SLEdge** is a lightweight serverless solution suitable for edge computing. It builds on WebAssembly sandboxing provided by the [aWsm compiler](https://github.com/gwsystems/aWsm).

## Setting up a development environment

### Native on Debian Host

```sh
git clone https://github.com/gwsystems/sledge-serverless-framework.git
cd sledge-serverless-framework
./install_deb.sh
source ~/.bashrc
make install
make test
```

### Docker

**Note: These steps require Docker. Make sure you've got it installed!**

[Docker Installation Instructions](https://docs.docker.com/install/)

We provide a Docker build environment configured with the dependencies and toolchain needed to build the SLEdge runtime and serverless functions.

To setup this environment, run:

```bash
./devenv.sh setup
```

### Using the Docker container to compile your serverless functions

To enter the docker environment, run:

```bash
./devenv.sh run
```

The first time you enter this environment, run the following to copy the sledgert binary to /sledge/runtime/bin.

```bash
cd /sledge/runtime
make clean all
```

There are a set of benchmarking applications in the `/sledge/applications` directory. Run the following to compile all benchmarks runtime tests using the aWsm compiler and then copy all resulting `<application>.wasm.so` files to /sledge/runtime/bin.

```bash
cd /sledge/applications/
make clean all
```

You now have everything that you need to execute your first serverless function on SLEdge

To exit the container:

```bash
exit
```

To stop the Docker container:

```bash
./devenv.sh stop
```

### Deleting Docker Build Containers

If you are finished working with the SLEdge runtime and wish to remove it, run the following command to delete our Docker build and runtime images.

```bash
./devenv.sh rma
```

And then simply delete this repository.

## Running your first serverless function

An SLEdge serverless function consists of a shared library (\*.so) and a JSON configuration file that determines how the runtime should execute the serverless function. As an example, here is the configuration file for our sample fibonacci function:

```json
{
  "name": "fibonacci",
  "path": "fibonacci.wasm.so",
  "port": 10000,
  "expected-execution-us": 600,
  "relative-deadline-us": 2000,
  "http-req-size": 1024,
  "http-resp-content-type": "text/plain"
}
```

The `port` and `name` fields are used to determine the path where our serverless function will be served served.

In our case, we are running the SLEdge runtime on localhost, so our function is available at `http://localhost:10000/fibonacci`

Our fibonacci function will parse a single argument from the HTTP POST body that we send. The expected Content-Type is "text/plain" and the buffer is sized to 1024 bytes for both the request and response. This is sufficient for our simple Fibonacci function, but this must be changed and sized for other functions, such as image processing.

Now that we understand roughly how the SLEdge runtime interacts with serverless function, let's run Fibonacci!

From the root project directory of the host environment (not the Docker container!), navigate to the binary directory

```bash
cd runtime/bin/
```

Now run the sledgert binary, passing the JSON file of the serverless function we want to serve. Because serverless functions are loaded by SLEdge as shared libraries, we want to add the `applications/` directory to LD_LIBRARY_PATH.

```bash
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_fibonacci.json
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

When done, terminal the SLEdge runtime with `Ctrl+c`

## Running Test Workloads

Various synthetic and real-world tests can be found in `runtime/tests`. Generally, each experiment can be run by Make rules in the top level `test.mk`.

`make -f test.mk all`

## Problems or Feedback?

If you encountered bugs or have feedback, please let us know in our [issue tracker.](https://github.com/gwsystems/sledge-serverless-framework/issues)
