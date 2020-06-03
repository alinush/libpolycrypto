#!/bin/bash

set -e

if [ $# -lt 4 ]; then
    echo "Runs the DKG benchmark for the specified DKG(s) with thresholds f+1 = {2, 4, 8, 16, 32, ...} and n = 2f+1"
    echo
    echo "Usage: $0 <machine-type> <dkg-type> <experiment-file> <out-file-name>"
    echo
    echo "OPTIONS:"
    echo "   <machine-type>     name of the machine you're running experiments on, to include in the output .csv and .log filenames"
    echo "   <dkg-types>        set to '' to run experiments for all DKG types or set to 'feld', 'kate', 'kate-sim', 'amt', 'amt-sim', 'fk' or 'fk-sim' (or spaces-separated combination) to run for that particular DKG type"
    echo "   <experiment-file>  file that specifies experiments"
    echo "   <out-directory>    CSV results will be stored in <out-directory>/all.csv (similarly for .log file)" 
    exit 1
fi

# Format: t n numDealIters numVerifyBcIters numVerifyWcIters numReconstrBcIters numReconstrWc scheme
#
# e.g.,:
# 2       32      10 100 100 0 0 kate-sim
# 64      64      10 100 50  0 0 kate-sim

if [ ! -f "$3" ]; then
    echo "ERROR: Could not open experiments file '$3'"
    exit 1
fi


scriptdir=$(cd $(dirname $0); pwd -P)
date=`date +%Y-%m-%d`
logfile=`mktemp`
machine=$1
dkg_types=$2
benchmarks=`cat "$3"`
outdirectory=$4

csvfile=$outdirectory/all.csv
logfile=$outdirectory/all.log

mkdir -p $outdirectory

if [ -f "$csvfile" ]; then
    echo "ERROR: Output CSV file '$csvfile' already exists"
    exit 1
fi

if [ -f "$logfile" ]; then
    echo "ERROR: Output log file file '$logfile' already exists"
    exit 1
fi

which BenchDKG 2>&1 >/dev/null || { echo "ERROR: You did not set up the environment"; exit 1; }

# default PP file for kate, fk, and amt
pp_file=$scriptdir/../../public-params/1048576/1048576

dkg_types=`echo $dkg_types | tr ',' ' '`

for dt in $dkg_types; do
    if [ "$dt" = "feld" ]; then
        :
    elif [ "$dt" = "kate" ]; then
        :
    elif [ "$dt" = "kate-sim" ]; then
        :
    elif [ "$dt" = "amt" ]; then
        :
    elif [ "$dt" = "amt-sim" ]; then
        :
    elif [ "$dt" = "fk" ]; then
        :
    elif [ "$dt" = "fk-sim" ]; then
        :
    else
        echo "ERROR: Unknown DKG type '$dt'"
        exit 1
    fi
done
    
echo "Logging to $logfile ..."

files=
numFiles=0
t1min=$((1048576*1024))
t2max=0
while read -r b; do
    # skip empty lines
    if [ -z "`echo -n $b`" ]; then 
        continue
    fi
    
    if echo $b | grep "^#" >/dev/null; then 
        echo "Skipping commented line: $b"
        continue
    fi

    t1=`echo $b | cut -d' ' -f 1`
    t2=`echo $b | cut -d' ' -f 2`
    numDealIters=`echo $b | cut -d' ' -f 3`
    numVerifyBcIters=`echo $b | cut -d' ' -f 4`
    numVerifyWcIters=`echo $b | cut -d' ' -f 5`
    numReconstrBcIters=`echo $b | cut -d' ' -f 6`
    numReconstrWcIters=`echo $b | cut -d' ' -f 7`
    dkg=`echo $b | cut -d' ' -f 8`

    if [ -n "$dkg_types" ]; then
        found=0
        for dt in $dkg_types; do
            if [ "$dkg" == "$dt" ]; then
                found=1
            fi
        done

        if [ $found -eq 0 ]; then
            echo "Skipping over excluded DKG type '$dkg'"
            continue
        fi
    fi

    echo "Running t \\in $t1 to $t2 $dkg DKG with $numDealIters dealing, $numVerifyBcIters/$numVerifyWcIters BC/WC verifying, $numReconstrBcIters/$numReconstrWcIters BC/WC reconstruction" | tee -a $logfile

    [ $t1 -lt $t1min ] && t1min=$t1
    [ $t2 -gt $t2max ] && t2max=$t2
    
    file=$outdirectory/${date}-${dkg}-dkg-${t1}-${t2}-${machine}.csv

    BenchDKG $pp_file $t1 $t2 $dkg $numDealIters \
        $numVerifyBcIters \
        $numVerifyWcIters \
        $numReconstrBcIters \
        $numReconstrWcIters \
        $file 2>&1 | tee -a $logfile

    files="$files $file"
    numFiles=$(($numFiles + 1))
done <<< "$benchmarks"

echo
echo "Merging files: "
echo
for f in $files; do echo "  $f"; done
echo
echo "...into: "
echo
echo "  $csvfile"
echo

(
    first_file=`echo $files | cut -f 1 -d' '`
    head -n 1 $first_file
    for f in $files; do
        tail -n +2 $f
    done
) >$csvfile
