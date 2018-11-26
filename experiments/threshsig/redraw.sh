#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

plot_cmd=$scriptdir/../../scripts/linux/plot-threshsig.py

rm -f $scriptdir/thresh.png 
$plot_cmd $scriptdir/thresh.png 0 0 1 $scriptdir/*naive*.csv $scriptdir/*efficient*.csv
#$plot_cmd $scriptdir/thresh-nodetail-naive.png 0 0 0 $scriptdir/*naive*.csv
#$plot_cmd $scriptdir/thresh-nodetail-naive+eff.png 0 0 0 $scriptdir/*naive*.csv $scriptdir/*efficient*.csv
