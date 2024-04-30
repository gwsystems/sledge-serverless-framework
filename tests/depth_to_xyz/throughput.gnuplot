reset

set term jpeg 
set output "throughput.jpg"

set xlabel "Reservation Utilization %"
set ylabel "Requests/sec"

set xrange [-5:]
set yrange [0:]

plot for [t_id in tenant_ids] 'throughput_'.t_id.'.dat' using 1:2 title 'Tenant '.t_id w lp

#plot 'throughput_A.dat' using 1:2 title 'Tenant A Throughput' linetype 1 linecolor 1 with linespoints,\
#     'throughput_B.dat' using 1:2 title 'Tenant B Throughput' linetype 2 linecolor 2 with linespoints
