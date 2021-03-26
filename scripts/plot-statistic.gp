reset
set xlabel 'F(n)'
set ylabel 'time (ns)'
set title 'Fibonacci runtime'
set term png enhanced font 'Verdana,10'
set output 'plot_statistic.png'
set grid
plot [0:500][0:350000] \
'plot_input_statistic' using 1:2 with linespoints linewidth 2 title "recursive",\
'' using 1:3 with linespoints linewidth 2 title "fast doubling"
