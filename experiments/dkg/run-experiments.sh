#!/bin/bash

set -e

if [ $# -lt 2 ]; then
    echo "Runs the DKG benchmark for the specified DKG(s) with thresholds f+1 = {2, 4, 8, 16, ..., 32768} and n = 2f+1"
    echo
    echo "Usage: $0 <machine-type> <dkg-type>"
    echo
    echo "OPTIONS:"
    echo "   <dkg-type> can be either 'kate', 'kate-sim', 'amt' or 'amt-sim'"
    exit 1
fi

scriptdir=$(cd $(dirname $0); pwd -P)
date=`date +%Y-%m-%d`
machine=$1
dkg_type=$2
pp_file=$scriptdir/../../public-params/1048576/1048576
multicore=0

which BenchDKG 2>&1 >/dev/null || { echo "ERROR: You did not set up the environment"; exit 1; }

if [ "$dkg_type" = "kate" ]; then
    :
elif [ "$dkg_type" = "kate-sim" ]; then
    :
elif [ "$dkg_type" = "amt" ]; then
    :
elif [ "$dkg_type" = "amt-sim" ]; then
    :
else
    echo "ERROR: Unknown DKG type '$dkg_type'"
    exit 1
fi

# Format: t n numDealIters numVerifyBcIters numVerifyWcIters numReconstrBcIters numReconstrWc scheme
benchmarks_kate_sim="\
2       32      10 100 100 0 0 kate-sim
64      64      10 100 50  0 0 kate-sim
128     128     10 100 25  0 0 kate-sim
256     256     10 100 12  0 0 kate-sim
512     512     10 50  10  0 0 kate-sim
1024    1024    10 25  10  0 0 kate-sim
2048    2048    10 12  10  0 0 kate-sim
4096    4096    10 10  5   0 0 kate-sim
8192    8192    10 10  3   0 0 kate-sim
16384   16384   10 10  2   0 0 kate-sim
32768   32768   10 10  1   0 0 kate-sim
65536   65536   10 10  1   0 0 kate-sim
131072  131072  5  10  1   0 0 kate-sim
262144  262144  4  10  1   0 0 kate-sim
524288  524288  3  10  1   0 0 kate-sim
1048576 1048576 2  10  1   0 0 kate-sim
"

benchmarks_kate="\
2     32    10 100 100 0 0 kate
64    64    10 100 50  0 0 kate
128   128   10 100 25  0 0 kate
256   256   10 100 12  0 0 kate
512   512   10 000 10  0 0 kate
1024  1024  8  000 10  0 0 kate
2048  2048  2  000 10  0 0 kate
4096  32768 1  000 10  0 0 kate
"

benchmarks_amt="\
2       32      10 100 100 0 0 amt
64      512     10 100 80  0 0 amt
1024    1024    10 100 40  0 0 amt
2048    2048    10 100 20  0 0 amt
4096    4096    10 100 8   0 0 amt
8192    8192    8  80  4   0 0 amt
16384   16384   5  40  2   0 0 amt
32768   32768   3  20  1   0 0 amt
65536   65536   3  16  1   0 0 amt
131072  131072  2  8   1   0 0 amt
262144  262144  1  4   1   0 0 amt
524288  524288  1  3   1   0 0 amt
1048576 1048576 1  2   1   0 0 amt
"

benchmarks="\
$benchmarks_kate_sim
$benchmarks_amt
"

files=
numFiles=0
t1min=$((1048576*1024))
t2max=0
while read -r b; do
    t1=`echo $b | cut -d' ' -f 1`
    t2=`echo $b | cut -d' ' -f 2`
    numDealIters=`echo $b | cut -d' ' -f 3`
    numVerifyBcIters=`echo $b | cut -d' ' -f 4`
    numVerifyWcIters=`echo $b | cut -d' ' -f 5`
    numReconstrBcIters=`echo $b | cut -d' ' -f 6`
    numReconstrWcIters=`echo $b | cut -d' ' -f 7`
    dkg=`echo $b | cut -d' ' -f 8`

    if [ "$dkg" != "$dkg_type" ]; then
        continue
    fi

    echo "Running t \\in $t1 to $t2 $dkg DKG with $numDealIters dealing and $numVerifyBcIters/$numVerifyWcIters BC/WC verifying"

    [ $t1 -lt $t1min ] && t1min=$t1
    [ $t2 -gt $t2max ] && t2max=$t2
    
    file=${date}-${dkg}-dkg-${t1}-${t2}-${machine}.csv
    logfile=${date}-${dkg}-dkg-${t1}-${t2}-${machine}.log
    echo "Logging to $logfile ..."
    if [ $multicore -eq 1 ]; then
        BenchDKG $pp_file $t1 $t2 $dkg $numDealIters \
            $numVerifyBcIters \
            $numVerifyWcIters \
            $numReconstrBcIters \
            $numReconstrWcIters \
            $file &>$logfile &
        sleep 1
    else
        BenchDKG $pp_file $t1 $t2 $dkg $numDealIters \
            $numVerifyBcIters \
            $numVerifyWcIters \
            $numReconstrBcIters \
            $numReconstrWcIters \
            $file 2>&1 | tee -a $logfile
    fi

    files="$files $file"
    logfiles="$logfiles $logfile"
    numFiles=$(($numFiles + 1))
done <<< "$benchmarks"

if [ $multicore -eq 1 ]; then
    echo "Waiting for processes to finish..."
    wait
fi

echo

if [ $numFiles -gt 1 ]; then
    # initially, no hours/minutes/seconds
    merged_file=${date}-${dkg_type}-dkg-${t1min}-${t2max}-${machine}.csv
    new_logfile=${date}-${dkg_type}-dkg-${t1min}-${t2max}-${machine}.log
    while [ -f "$merged_file" -o -f "$new_logfile" ]; do
        # but if file name clashes, add hours/minutes/seconds
        extra_date=`date +%H-%M-%S`
        merged_file=${date}-${extra_date}-${dkg_type}-dkg-${t1min}-${t2max}-${machine}.csv
        new_logfile=${date}-${extra_date}-${dkg_type}-dkg-${t1min}-${t2max}-${machine}.log
    done

    cat $logfiles >"$new_logfile"
    (
        first_file=`echo $files | cut -f 1 -d' '`
        head -n 1 $first_file
        for f in $files; do
            tail -n +2 $f
        done
    ) >$merged_file
    rm $files $logfiles

    echo "Merged files: "
    echo
    for f in $files; do echo "  $f"; done
    echo
    echo "...into: "
    echo
    echo "  $merged_file"
    echo
fi
