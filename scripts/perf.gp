reset
set xlabel 'fibonacci input'
set ylabel 'time (ns)'
set title 'fibdrv single calculation'
set term png enhanced font 'Verdana,10'
set output 'perf_measure.png'
set key left

plot [0:120][0:8000] \
'orig_downward_perf_res.txt' using 1:2 with points title 'orig- down cnt',\
'orig_upward_perf_res.txt' using 1:2 with points title 'orig- up cnt',\
'' using 1:3 with points title 'orig- kernel calculation',\
'' using 1:4 with points title 'orig- total transmission',\
'bignum_downward_perf_res.txt' using 1:2 with points title 'big- down cnt',\
'bignum_upward_perf_res.txt' using 1:2 with points title 'big- up cnt',\
'' using 1:3 with points title 'big- kernel calculation',\
'' using 1:4 with points title 'big- total transmission'
