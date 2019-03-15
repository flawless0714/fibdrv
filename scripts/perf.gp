reset
set xlabel 'fibonacci input'
set ylabel 'time (ns)'
set title 'fibdrv single calculation'
set term png enhanced font 'Verdana,10'
set output 'perf_measure.png'

plot [0:110][0:1600] \
'orig_downward_perf_res.txt' using 1:2 with points title 'down cnt',\
'orig_upward_perf_res.txt' using 1:2 with points title 'up cnt',\
'' using 1:3 with points title 'kernel calculation',\
'' using 1:4 with points title 'total transmission',\
