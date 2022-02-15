reset

set term jpeg size 1200,800
set output "success_all.jpg"

set xlabel "Replenishment Period (ms)"
set ylabel "Deadline success rate %"

set xrange [0:]
set yrange [0:110]

set style histogram columnstacked
set key horizontal

plot for [i=2:*] 'success.dat' using 1:i title columnhead linetype 1 linecolor i with linespoints
