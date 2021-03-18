# EKF

Executes TinyEKF as shows by [You Chong's GPS example](http://www.mathworks.com/matlabcentral/fileexchange/31487-extended-kalman-filter-ekf--for-gps)

This test executes multiple runs of three iterations (the output of a previous iteration is refed as input), comparing the binary result against a known memoized result stored in `initial_state.dat`, `one_iteration.dat`, `two_iterations.dat`, and `three_iterations.dat`.

The `rust.sh` script stores per-run results to temporary files suffixed with `*.res.dat` and `diff -s` because I had trouble using cURL to pass a bash string of binary input.

In order to be compatible with the stdin/stdout model of serverless, the input and output files are binary concatenations of various C structs.

See `main()` in `runtime/tests/TinyEKF/extras/c/gps_ekf_fn.c` for specifics.

## Useful parsing of the log

_What is the average execution time of the first iteration?_
cat log.csv | grep ekf_first_iter | awk -F , '{ sum+=$5 } END{ print sum/NR }'
