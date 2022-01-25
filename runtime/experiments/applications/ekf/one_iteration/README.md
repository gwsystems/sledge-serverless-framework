# EKF

Executes TinyEKF as shows by [You Chong's GPS example](http://www.mathworks.com/matlabcentral/fileexchange/31487-extended-kalman-filter-ekf--for-gps)

In order to be compatible with the stdin/stdout model of serverless, the input and output files are binary concatenations of various C structs.

See `main()` in `applications/TinyEKF/extras/c/gps_ekf_fn.c` for specifics.

This test executes multiple iterations, comparing the binary result against a known memoized result stored at `expected_result.dat`.
