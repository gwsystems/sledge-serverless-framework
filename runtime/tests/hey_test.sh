./test_rps.sh 105.txt 60 100 105k.jpg 10000 2>&1 &
pid1=$!
./test_rps.sh 305.txt 60 100 305k.jpg 10003 2>&1 &
pid2=$!
./test_rps.sh 5.txt 60 100 5k.jpg 10006 2>&1 &
pid3=$!
./test_rps.sh 40.txt 60 100 40k.jpg 10009 2>&1 &
pid4=$!
wait -f $pid1
wait -f $pid2
wait -f $pid3
wait -f $pid4
printf "[OK]\n"

