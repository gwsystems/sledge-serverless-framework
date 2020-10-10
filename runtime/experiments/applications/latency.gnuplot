reset

set term jpeg 
set output "latency.jpg"

set xlabel "Connections"
set xrange [-5:105]

set ylabel "Latency (ms)"
set yrange [0:]

set key left top


set style histogram columnstacked

plot 'latency.dat' using 1:2 title 'p50', \
     'latency.dat' using 1:3 title 'p90', \
     'latency.dat' using 1:4 title 'p99', \
     'latency.dat' using 1:5 title 'p100', \
