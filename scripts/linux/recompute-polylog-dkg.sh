#!/bin/bash

set -e

scriptdir=$(cd $(dirname $0); pwd -P)
. $scriptdir/shlibs/os.sh

out_file_name=$scriptdir/../../experiments/polylog-dkg

for secparam in 40 60 80 100 128; do
    out_file="${out_file_name}-${secparam}.csv"
    rm -f $out_file

    n=1024
    for i in `seq 1 14`; do 
        n=$((n*2))
        echo "Getting numbers for lambda = $secparam and n = $n ..."
        if ! PolylogDkg $secparam $n $out_file; then #&>/dev/null; then
            echo "ERROR: Failed generating params for current config"
            exit 1
        fi
    done
done
