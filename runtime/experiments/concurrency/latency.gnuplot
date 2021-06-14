reset

set term jpeg 
set output "latency.jpg"

set xlabel "Concurrency"
set ylabel "Latency (ms)"

set key left top

set xrange [-5:105]
set yrange [0:]

set style histogram columnstacked

plot 'latency.dat' using 1:8 title 'p100', \
     'latency.dat' using 1:7 title 'p99', \
     'latency.dat' using 1:6 title 'p90', \
     'latency.dat' using 1:5 title 'p50', \
     'latency.dat' using 1:4 title 'mean', \
     'latency.dat' using 1:3 title 'min', \
