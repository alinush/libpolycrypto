#!/bin/sh

scriptdir=$(cd $(dirname $0); pwd -P)

prefix=${1:-""}

plot_dkg_cmd=$scriptdir/../../../scripts/linux/plot-vss-times.py
plot_speedup_cmd=$scriptdir/../../../scripts/linux/plot-fk-speedup.py

$plot_dkg_cmd \
    "" 0 0 1 \
    $scriptdir/../amt-dkg*macbook*.csv $scriptdir/../fk-dkg*macbook*.csv

$plot_speedup_cmd \
    "" 0 0 1 \
    $scriptdir/../amt-dkg*macbook*.csv $scriptdir/../fk-dkg*macbook*.csv
