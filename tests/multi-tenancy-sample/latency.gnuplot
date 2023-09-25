reset

set term jpeg size 1000,500
set output "latency.jpg"

#set xlabel "Reservation Utilization %"
#set ylabel "Latency (us)"

set key left top

set xrange [-5:]
set yrange [0:]

set style histogram columnstacked
set key horizontal

set macros
# Placement of the a,b,c,d labels in the graphs
POS = "at graph 0.05,1.03 font ',10'"

# x- and ytics for each row resp. column
NOXTICS = "unset xlabel"
XTICS = "set xlabel 'Reservation Utilization %'"
NOYTICS = "unset ylabel"
YTICS = "set ylabel 'Latency (us)'"

# Margins for each row resp. column
TMARGIN = "set tmargin at screen 0.90; set bmargin at screen 0.55"
BMARGIN = "set tmargin at screen 0.55; set bmargin at screen 0.20"
LMARGIN = "set lmargin at screen 0.15; set rmargin at screen 0.55"
RMARGIN = "set lmargin at screen 0.55; set rmargin at screen 0.95"

# plot \
#      for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:7 title 'Tenant '.t_id.' p99'  w lp, \
#      for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:6 title 'Tenant '.t_id.' p90'  w lp, \
#      for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:5 title 'Tenant '.t_id.' p50'  w lp, \
#      for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:4 title 'Tenant '.t_id.' mean'  w lp, \
#      for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:3 title 'Tenant '.t_id.' min'  w lp

### Start multiplot (2x2 layout)
set multiplot layout 2,2 rowsfirst
# --- GRAPH a
set label 1 'p99' @POS
@NOXTICS; @YTICS
#@TMARGIN; @LMARGIN
plot for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:7 title 'Tenant '.t_id  w lp
# --- GRAPH b
set label 1 'p90' @POS
@NOXTICS; @NOYTICS
#@TMARGIN; @RMARGIN
plot for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:6 notitle  w lp
# --- GRAPH c
set label 1 'p50' @POS
@XTICS; @YTICS
#@BMARGIN; @LMARGIN
plot for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:5 notitle w lp
# --- GRAPH d
set label 1 'mean' @POS
@XTICS; @NOYTICS
#@BMARGIN; @RMARGIN
plot for [t_id in tenant_ids] 'latency_'.t_id.'.dat' using 1:4 notitle w lp
unset multiplot
### End multiplot

# plot \
#      'latency_A.dat' using 1:7 title 'A p99' lt 1 lc 1 w lp, \
#      'latency_A.dat' using 1:6 title 'A p90' lt 2 lc 1 w lp, \
#      'latency_A.dat' using 1:5 title 'A p50' lt 3 lc 1 w lp, \
#      'latency_A.dat' using 1:4 title 'A mean' lt 4 lc 1 w lp, \
#      'latency_A.dat' using 1:3 title 'A min' lt 5 lc 1 w lp,\
#      'latency_B.dat' using 1:7 title 'B p99' lt 1 lc 2 w lp, \
#      'latency_B.dat' using 1:6 title 'B p90' lt 2 lc 2 w lp, \
#      'latency_B.dat' using 1:5 title 'B p50' lt 3 lc 2 w lp, \
#      'latency_B.dat' using 1:4 title 'B mean' lt 4 lc 2 w lp, \
#      'latency_B.dat' using 1:3 title 'B min' lt 5 lc 2 w lp

#    'latency_A.dat' using 1:8 title 'A p100' linetype 0 linecolor 1 with linespoints, \
#    'latency_B.dat' using 1:8 title 'B p100' linetype 0 linecolor 2 with linespoints, \
