#!/bin/sh

scriptdir=$(cd $(dirname $0); pwd -P)

prefix=${1:-""}

plot_cmd=$scriptdir/../../scripts/linux/plot-dkg-times.py

png_prefix=dkg

rm -f ${png_prefix}*.png

# Just plots Kate vs AMT
$plot_cmd \
    $png_prefix 2 0 \
    $scriptdir/*${prefix}*kate*dvorak*.csv \
    $scriptdir/*${prefix}*amt*dvorak*.csv

# Just plots Kate
#$plot_cmd \
#    "$png_prefix-ejf" 2 0 \
#    $scriptdir/*${prefix}*kate*dvorak*.csv \
