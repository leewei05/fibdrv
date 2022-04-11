reset
set output 'f.png'
set xlabel 'fib(n)'
set ylabel 'time(ns)'
set title 'fibdrv performance'
set term png enhanced font 'Verdana,10'

plot [0:100][0:1000] \
'plot' using 1:2 with linespoints linewidth 2 title "kernel space time", \
'plot' using 1:3 with linespoints linewidth 2 title "user space time", \
'plot' using 1:4 with linespoints linewidth 2 title "kernel to user space time", \

