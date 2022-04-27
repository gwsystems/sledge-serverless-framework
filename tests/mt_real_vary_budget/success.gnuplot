reset

set term jpeg 
set output "success.jpg"

set xlabel "Rerervation Utilization %"
set ylabel "Deadline success rate %"

set xrange [-5:105]
set yrange [0:110]

plot 'success.dat' using 1:2 title 'Deadline success rate' linetype 1 linecolor 1 with linespoints
