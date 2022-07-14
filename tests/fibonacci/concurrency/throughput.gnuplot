reset

set term jpeg 
set output "throughput.jpg"

set xlabel "Concurrency"
set ylabel "Requests/sec"

set xrange [-5:]
set yrange [0:]

plot 'throughput.dat' using 1:2 title 'Reqs/sec' linetype 1 linecolor 1 with linespoints
