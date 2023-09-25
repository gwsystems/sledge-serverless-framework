reset

set term jpeg 
set output "success.jpg"

set xlabel "Reservation Utilization %"
set ylabel "Deadline success rate %"

set xrange [-5:]
set yrange [0:110]

plot for [t_id in tenant_ids] 'success_'.t_id.'.dat' using 1:2 title t_id w lp

#plot 'success_A.dat' using 1:2 title 'Tenant A success rate' linetype 1 linecolor 1 with linespoints,\
#     'success_B.dat' using 1:2 title 'Tenant B success rate' lt 2 lc 2 w lp
