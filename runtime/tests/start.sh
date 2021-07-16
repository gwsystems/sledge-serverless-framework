function usage {
        echo "$0 [perf output file, chain_function_perf.log or single_function_perf.log]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

output=$1


declare project_path="$(
        cd "$(dirname "$1")/../.."
        pwd
)"
echo $project_path
export SLEDGE_SANDBOX_PERF_LOG=$project_path/runtime/tests/$output
cd ../bin
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_big_fibonacci.json
LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_fibonacci.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/test_sodresize.json
#LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH" ./sledgert ../tests/my_sodresize.json
