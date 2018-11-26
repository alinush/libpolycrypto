#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

plot_cmd=$scriptdir/../../scripts/linux/plot-vss-times.py 

rm -f $scriptdir/*.png

#$plot_cmd \
#    "-evss-only" 2  0 \
#    $scriptdir/kate.csv

(
cd $scriptdir;
$plot_cmd \
    "" 2  0 \
    kate.csv \
    amt.csv \
)

#$plot_cmd \
#    "-with-feld" 31 0 \
#    $scriptdir/kate.csv \
#    $scriptdir/amt.csv \
#    $scriptdir/feld.csv \
