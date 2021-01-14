#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

plot_vss_cmd=$scriptdir/../../../scripts/linux/plot-vss-times.py 
plot_speedup_cmd=$scriptdir/../../../scripts/linux/plot-fk-speedup.py 
plot_speedup_and_down_cmd=$scriptdir/../../../scripts/linux/plot-fk-speedup-and-down.py 

rm -f $scriptdir/*.png

minN=0
maxN=0
hasLogscale=1

(
cd $scriptdir;
$plot_vss_cmd \
    "" $minN $maxN $hasLogscale \
    $scriptdir/../amt-fk-macbook.csv

$plot_speedup_cmd \
    "" $minN $maxN $hasLogscale \
    $scriptdir/../amt-fk-macbook.csv


$plot_speedup_and_down_cmd \
    "" $minN $maxN $hasLogscale \
    $scriptdir/../amt-fk-macbook.csv
)
