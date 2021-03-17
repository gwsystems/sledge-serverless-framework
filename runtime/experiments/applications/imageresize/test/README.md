# Resize Images

This script runs several test executions of the resize application, testing that the output is pixel-for-pixel identical to a known good output.

The workload works sporadically, but the runtime errors out due to calls to `mremap`. The runtime gratuitously logs these calls for the time being.
