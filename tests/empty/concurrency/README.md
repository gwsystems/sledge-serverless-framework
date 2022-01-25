# Concurrency

## Question

_How does increasing levels of concurrent client requests affect tail latency, throughput, and the success/error rate of sandbox execution?_

## Independent Variable

- The number of concurrent client requests made at a given time

## Dependent Variables

- p50, p90, p99, and p100 latency measured in ms
- throughput measures in requests/second
- success rate, measures in % of requests that return a 200

## Assumptions about test environment

- You have a modern bash shell. My Linux environment shows version 4.4.20(1)-release
- `hey` (https://github.com/rakyll/hey) is available in your PATH
- You have compiled `sledgert` and the `empty.wasm.so` test workload

## TODO

- Harden scripts to validate assumptions
- Improve error handling in scripts. If `sledgrt` crashes, this charges forward until it hits a divide by error when attempting to clean data that doesn't exist
