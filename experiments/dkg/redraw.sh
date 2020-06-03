#!/bin/sh

scriptdir=$(cd $(dirname $0); pwd -P)

plot_cmd=$scriptdir/../../scripts/linux/plot-vss-times.py

# Just plots Kate vs AMT vs FK
$plot_cmd \
    "" 0 0 1 \
    $scriptdir/kate*dvorak*.csv \
    $scriptdir/amt*macbook*.csv \
    $scriptdir/fk*macbook*.csv

# Just plots AMT vs FK
$plot_cmd \
    "-fk-amt" 0 0 1 \
    $scriptdir/fk*macbook*.csv \
    $scriptdir/amt*macbook*.csv
