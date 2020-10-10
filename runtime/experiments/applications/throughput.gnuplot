reset

set term jpeg 
set output "throughput.jpg"

# TODO: Axis shouldn't be linear
set xlabel "Connections"
set xrange [-5:105]

set ylabel "Requests/sec"
set yrange [0:]

plot 'throughput.dat' using 1:2 title 'Reqs/sec'
