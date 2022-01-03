reset

set term jpeg 
set output "success.jpg"

set xlabel "Concurrency"
set ylabel "% 2XX"

set xrange [-5:105]
set yrange [0:110]

plot 'success.dat' using 1:2 title '2XX' linetype 1 linecolor 1 with linespoints
