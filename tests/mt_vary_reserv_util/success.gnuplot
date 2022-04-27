reset

set term jpeg 
set output "success.jpg"

set xlabel "Replenishment Period (ms)"
set ylabel "Deadline success rate %"

set xrange [0:]
set yrange [0:105]

plot 'success.dat' using 1:2 title 'Deadline success rate' linetype 1 linecolor 1 with linespoints
