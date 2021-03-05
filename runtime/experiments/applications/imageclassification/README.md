# Image Classification

Classifies a tiny 32x32 image from the CIFAR-10 dataset into one of ten different classes:
airplane
automobile
bird
cat
deer
dog
frog
horse
ship
truck

An initial run incorrectly classified all images as class 8. It seems that this was because the image data was not being read correctly. @emil916 created a fix, but the performance on the images pulled from the classifier's associated website still seems poor and is captured in `erroneous_output.txt`.
