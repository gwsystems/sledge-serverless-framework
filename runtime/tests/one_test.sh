 ./test.sh small.txt 2 480 9 2 2>&1 &
pid1=$!
 ./test.sh large.txt 1 120 9 2 2>&1 & 
pid2=$!
wait -f $pid1
wait -f $pid2
printf "[OK]\n"


