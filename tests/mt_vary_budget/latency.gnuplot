reset

set term jpeg 
set output "latency.jpg"

set xlabel "Rerervation Utilization %"
set ylabel "Latency (us)"

set key left top

set xrange [-5:105]
set yrange [0:]

set style histogram columnstacked

plot 'latency.dat' using 1:8 title 'p100' linetype 0 linecolor 0 with linespoints, \
     'latency.dat' using 1:7 title 'p99' lt 1 lc 1 w lp, \
     'latency.dat' using 1:6 title 'p90' lt 2 lc 2 w lp, \
     'latency.dat' using 1:5 title 'p50' lt 3 lc 3 w lp, \
     'latency.dat' using 1:4 title 'mean' lt 4 lc 4 w lp, \
     'latency.dat' using 1:3 title 'min' lt 5 lc 5 w lp
