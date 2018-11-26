#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

#echo "Script dir: $scriptdir"

# DKG dealing times are basically the same as eVSS dealing times
#$scriptdir/../scripts/linux/plot-deal-times.py "-with-feld" 0 0 vss/kate.csv vss/feld.csv vss/amt.csv;
$scriptdir/../scripts/linux/plot-deal-times.py "" 0 0 vss/kate.csv vss/amt.csv;
