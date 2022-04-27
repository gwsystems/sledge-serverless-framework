reset

set term jpeg 
set output "throughput.jpg"

set xlabel "Rerervation Utilization %"
set ylabel "Requests/sec"

set xrange [-5:105]
set yrange [0:]

plot 'throughput.dat' using 1:2 title 'Throughput (reqs/sec)' linetype 1 linecolor 1 with linespoints
