./test_concurrency.sh 105.txt 5000 100 105k.jpg 10000 2>&1 &
pid1=$!
./test_concurrency.sh 305.txt 5000 100 305k.jpg 10003 2>&1 &
pid2=$!
./test_concurrency.sh 5.txt 5000 100 5k.jpg 10006 2>&1 &
pid3=$!
./test_concurrency.sh 40.txt 5000 100 40k.jpg 10009 2>&1 &
pid4=$!
wait -f $pid1
wait -f $pid2
wait -f $pid3
wait -f $pid4
printf "[OK]\n"

