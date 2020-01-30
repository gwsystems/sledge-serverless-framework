#!/bin/bash
find ./runtime -type f -path "./*.[ch]" |                             # Match all *.c and *.h files in ./runtime
  grep --invert-match -E "./runtime/jsmn/*|./runtime/http-parser/*" | # Excluding those in the jsmn or http-parser submodules
  xargs clang-format -i                                               # And format them with clang-format
