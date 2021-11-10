function usage {
        echo "$0 [duration(s)] [concurrency]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

duration=$1
concurrency=$2

f1="105k_"$concurrency".txt"
echo $f1
f2="305k_"$concurrency".txt"
f3="5k_"$concurrency".txt"
f4="40k_"$concurrency".txt"

./test_8c.sh $f1 $duration $concurrency 105k.jpg 10000 2>&1 &
pid1=$!
./test_8c.sh $f2 $duration $concurrency 305k.jpg 10003 2>&1 &
pid2=$!
./test_8c.sh $f3 $duration $concurrency 5k.jpg 10006 2>&1 &
pid3=$!
./test_8c.sh $f4 $duration $concurrency 40k.jpg 10009 2>&1 &
pid4=$!
wait -f $pid1
wait -f $pid2
wait -f $pid3
wait -f $pid4
printf "[OK]\n"

