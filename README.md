# aWsm (awesome)

**aWsm** is an efficient WASM runtime built with the `silverfish` compiler. This is an active research effort with regular breaking changes and no guarantees of stability.

## Host Dependencies

- Docker

Additionally, if you want to execute the Awsm runtime on your host environment, you need libuv. A reason you might want to do this is to debug your serverless function, as GDB does not seem to run properly within a Docker container.

If on Debian, you can install libuv with the following:

```bash
./devenv.sh install_libuv
```

## Setting up the environment

**Note: These steps require Docker. Make sure you've got it installed!**

We provide a Docker build environment configured with the dependencies and toolchain needed to build the Awsm runtime and serverless functions.

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

We can now run Awsm with one of the serverless functions we built. Let's run Fibonacci!

Because serverless functions are loaded by Aswsm as shared libraries, we want to add the `runtime/tests/` directory to LD_LIBRARY_PATH.

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

Let's get the 10th. Note that I'm using ApacheBench to make this request.

Note: You possibly run the awsmrt command in the foreground. If so, you should open a new terminal session.

```bash
echo 10 >fib.txt
ab -c 1 -n 1 -p fib.txt -v 2 http://localhost:10000/fibonacci
```

In my case, I received the following in response. The response is 55, which seems to be correct!

```bash
This is ApacheBench, Version 2.3 <$Revision: 1807734 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking localhost (be patient)...INFO: POST header ==
---
POST /fibonacci HTTP/1.0
Content-length: 3
Content-type: text/plain
Host: localhost:10000
User-Agent: ApacheBench/2.3
Accept: */*


---
LOG: header received:
HTTP/1.1 200 OK
Content-type: text/plain
Content-length: 3

55

..done


Server Software:
Server Hostname:        localhost
Server Port:            10000

Document Path:          /fibonacci
Document Length:        3 bytes

Concurrency Level:      1
Time taken for tests:   0.001 seconds
Complete requests:      1
Failed requests:        0
Total transferred:      100 bytes
Total body sent:        141
HTML transferred:       3 bytes
Requests per second:    952.38 [#/sec] (mean)
Time per request:       1.050 [ms] (mean)
Time per request:       1.050 [ms] (mean, across all concurrent requests)
Transfer rate:          93.01 [Kbytes/sec] received
                        131.14 kb/s sent
                        224.14 kb/s total

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        1    1   0.0      1       1
Processing:     0    0   0.0      0       0
Waiting:        0    0   0.0      0       0
Total:          1    1   0.0      1       1
```

## Stopping the Runtime

When you are finished, stop Awsm with

```bash
./devenv.sh stop
```
