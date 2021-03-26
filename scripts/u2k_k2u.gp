reset
set xlabel 'F(n)'
set ylabel 'time (ns)'
set title 'System call overhead 2'
set term png enhanced font 'Verdana,10'
set output 'u2k_k2u.png'
set grid
plot [0:100][0:300] \
'u2k_k2u' using 1:2 with linespoints linewidth 2 title "user to kernel",\
'' using 1:3 with linespoints linewidth 2 title "kernel to user",\
