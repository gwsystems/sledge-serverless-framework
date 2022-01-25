reset

set term jpeg 
set output "success.jpg"

set xlabel "Payload (bytes)"
set xrange [-5:1050000]

set ylabel "% 2XX"
set yrange [0:110]

plot 'success.dat' using 1:2 title '2XX'
