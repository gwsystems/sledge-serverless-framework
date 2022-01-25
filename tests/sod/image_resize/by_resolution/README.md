# Resize Image by Resolution

The goal of this experiment is to run the resize operation on small, medium, and large source images and measure how the size of the input image affects execution time.

The workload works sporadically, but the runtime errors out due to calls to `mremap`. The runtime gratuitously logs these calls for the time being.
