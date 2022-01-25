# License Plate Detection

Originally, this workload returned an image with license plates annotated with magenta bounding boxes. It was modified to return a textual representation of the bounding boxed to reduce network payload. However, the resulting coordinates seem to not match the coordinates of the source dataset suggesting that the image may have been resized or something.

Solving the problem is deferred to a low priority task because we previously verified that the workload is running when it returned annotated images.

The scripts DO NOT check for functional correctness of output for this reason. Please look at other scripts to have a better example for new applications.
