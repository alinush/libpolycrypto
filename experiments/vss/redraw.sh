#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

plot_cmd=$scriptdir/../../scripts/linux/plot-vss-times.py 

rm -f $scriptdir/*.png

minN=2
maxN=0
hasLogscale=1

(
cd $scriptdir;
$plot_cmd \
    "" $minN $maxN $hasLogscale \
    kate.csv \
    amt.csv \
    fk.csv \
)

(
cd $scriptdir;
$plot_cmd \
    "small-scale" 2 1024 $hasLogscale \
    kate.csv \
    amt.csv \
    fk.csv \
)
