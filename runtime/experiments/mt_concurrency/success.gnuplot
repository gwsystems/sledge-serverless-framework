reset

set term jpeg 
set output "success.jpg"

set xlabel "Concurrency"
set ylabel "Success (RC=200) rate %"

set xrange [-5:105]
set yrange [0:110]

plot 'success.dat' using 1:2 title 'Success rate' linetype 1 linecolor 1 with linespoints
