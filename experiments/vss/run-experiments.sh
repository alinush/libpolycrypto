#!/bin/bash

set -e

if [ $# -lt 4 ]; then
    echo "Runs the VSS benchmark for the specified VSS(s) with thresholds f+1 = {2, 4, 8, 16, 32, ...} and n = 2f+1"
    echo
    echo "Usage: $0 <machine-type> <vss-type> <experiment-file> <out-file-name>"
    echo
    echo "OPTIONS:"
    echo "   <machine-type>     name of the machine you're running experiments on, to include in the output .csv and .log filenames"
    echo "   <vss-type>         set to '' to run experiments for all VSS types or set to 'feld', 'kate', 'amt', or 'fk' to run for that particular VSS type"
    echo "   <experiment-file>  file that specifies experiments"
    echo "   <out-directory>    CSV results will be stored in <out-directory>/all.csv (similarly for .log file)" 
    exit 1
fi

# NOTE: Feldman VSS needs more players created to get a good average of the verification time
# Format: <min-t> <max-t> <num-deal-iters> <num-verify-iters> <num-reconstr-iters>
#
# e.g.,
#
# 1     16    100 1000 1000 feld
# 2     32    10  1000 1000 kate
# 64    64    10  1000 500  kate

if [ ! -f "$3" ]; then
    echo "ERROR: Could not open experiments file '$3'"
    exit 1
fi

scriptdir=$(cd $(dirname $0); pwd -P)
date=`date +%Y-%m-%d`
logfile=`mktemp`
machine=$1
vss_type=$2
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

which BenchVSS || { echo "ERROR: You did not set up the environment"; exit 1; }

# default PP file for kate, fk, and amt
pp_file=$scriptdir/../../public-params/1048576/1048576

if [ "$vss_type" = "feld" ]; then
    pp_file="/dev/null"
elif [ "$vss_type" = "kate" ]; then
    :
elif [ "$vss_type" = "fk" ]; then
    :
elif [ "$vss_type" = "kate-sim" ]; then
    :
elif [ "$vss_type" = "amt" ]; then
    :
elif [ -z "$vss_type" ]; then
    :
else
    echo "ERROR: Unknown VSS type '$vss_type'"
    exit 1
fi

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
    numVerIters=`echo $b | cut -d' ' -f 4`
    numReconstrIters=`echo $b | cut -d' ' -f 5`
    vss=`echo $b | cut -d' ' -f 6`

    # if $vss_type is empty, runs for all VSS types
    if [ -n "$vss_type" -a "$vss" != "$vss_type" ]; then
        continue
    fi

    echo "Running t \\in $t1 to $t2 $vss VSS with $numDealIters deal iters, $numVerIters verif. iters and $numReconstrIters reconstr. iters" | tee -a $logfile

    [ $t1 -lt $t1min ] && t1min=$t1
    [ $t2 -gt $t2max ] && t2max=$t2
    
    file=$outdirectory/${date}-${vss}-vss-${t1}-${t2}-${machine}.csv

    BenchVSS $pp_file $t1 $t2 $vss $numDealIters $numVerIters $numReconstrIters $file 2>&1 | tee -a $logfile

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
