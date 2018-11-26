#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

plot_cmd=$scriptdir/../../scripts/linux/plot-bandwidth.py

rm -f $scriptdir/bw-ejf-vs-amt.png

$plot_cmd \
    $scriptdir/bw-ejf-vs-amt.png \
    "AMT DKG,eJF-DKG" 0 0 \
    $scriptdir/bandwidth-1M.csv
