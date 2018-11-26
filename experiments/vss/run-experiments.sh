#!/bin/bash

set -e

if [ $# -lt 2 ]; then
    echo "Runs the VSS benchmark for the specified VSS(s) with thresholds f+1 = {2, 4, 8, 16, ..., 32768} and n = 2f+1"
    echo
    echo "Usage: $0 <machine-type> <vss-type>"
    echo
    echo "OPTIONS:"
    echo "   <vss-type> can be either 'feld', 'kate' or 'amt'"
    exit 1
fi

scriptdir=$(cd $(dirname $0); pwd -P)
date=`date +%Y-%m-%d`
logfile=`mktemp`
machine=$1
vss_type=$2

which BenchVSS || { echo "ERROR: You did not set up the environment"; exit 1; }

if [ "$vss_type" = "feld" ]; then
    pp_file="/dev/null"
elif [ "$vss_type" = "kate" ]; then
    pp_file=$scriptdir/../../public-params/1048576/1048576
elif [ "$vss_type" = "kate-sim" ]; then
    pp_file=$scriptdir/../../public-params/1048576/1048576
elif [ "$vss_type" = "amt" ]; then
    pp_file=$scriptdir/../../public-params/1048576/1048576
else
    echo "ERROR: Unknown VSS type '$vss_type'"
    exit 1
fi

# Format: <min-t> <max-t> <num-deal-iters> <num-verify-iters> <num-reconstr-iters>

# NOTE: Feldman VSS needs more players created to get a good average of the verification time
feld_benchmarks="\
2       16      100 1000 1000 feld
32      32      100 1000 200  feld
64      64      100 1000 50   feld
128     128     100 1000 25   feld
256     256     100 1000 10   feld
512     512     100 1000 10   feld
1024    1024    50  1000 5    feld
2048    2048    20  500  2    feld
4096    4096    20  250  2    feld
8192    8192    10  120  2    feld
16384   16384   10  60   1    feld
32768   32768   10  30   0    feld
65536   65536   10  15   0    feld
131072  131072  10  10   0    feld
262144  262144  10  10   0    feld
524288  524288  10  8    0    feld
1048576 1048576 10  5    0    feld
"
kate_benchmarks="\
2     32    10  1000 1000 kate
64    64    10  1000 500  kate
128   128   10  1000 250  kate
256   256   10  1000 120  kate
512   512   10  1000 60   kate
1024  1024  10  1000 30   kate
2048  2048  3   1000 15   kate
4096  4096  2   1000 10   kate
8192  8192  2   1000 10   kate
16384 32768 1   1000 5    kate
"
kate_sim_benchmarks="\
2       32      100 1000 1000 kate-sim
64      64      100 1000 500  kate-sim
128     128     100 1000 250  kate-sim
256     256     100 1000 120  kate-sim
512     512     10  1000 60   kate-sim
1024    1024    10  1000 30   kate-sim
2048    2048    10  1000 15   kate-sim
4096    4096    10  1000 10   kate-sim
8192    8192    10  1000 10   kate-sim
16384   32768   10  1000 10   kate-sim
65536   65536   10  1000 10   kate-sim
131072  131072  5   1000 8    kate-sim
262144  262144  3   1000 4    kate-sim
524288  524288  3   1000 2    kate-sim
1048576 1048576 2   1000 1    kate-sim
"
amt_benchmarks="\
2       512     100 1000 100 amt
1024    4096    100 1000 10  amt
8192    8192    50  1000 10  amt
16384   16384   22  1000 4   amt
32768   32768   10  1000 2   amt
65536   65536   5   1000 1   amt
262144  262144  2   1000 1   amt
524288  524288  1   1000 1   amt
1048576 1048576 1   1000 1   amt
"
benchmarks="\
$amt_benchmarks
$kate_benchmarks
$kate_sim_benchmarks
$feld_benchmarks
"

files=
numFiles=0
t1min=$((1048576*1024))
t2max=0
while read -r b; do
    t1=`echo $b | cut -d' ' -f 1`
    t2=`echo $b | cut -d' ' -f 2`
    numDealIters=`echo $b | cut -d' ' -f 3`
    numVerIters=`echo $b | cut -d' ' -f 4`
    numReconstrIters=`echo $b | cut -d' ' -f 5`
    vss=`echo $b | cut -d' ' -f 6`

    if [ "$vss" != "$vss_type" ]; then
        continue
    fi

    echo "Running t \\in $t1 to $t2 $vss VSS with $numDealIters iters for dealing"

    [ $t1 -lt $t1min ] && t1min=$t1
    [ $t2 -gt $t2max ] && t2max=$t2
    
    file=${date}-${vss}-vss-${t1}-${t2}-${machine}.csv

    echo "Logging to $logfile ..."
    BenchVSS $pp_file $t1 $t2 $vss $numDealIters $numVerIters $numReconstrIters $file 2>&1 | tee -a $logfile

    files="$files $file"
    numFiles=$(($numFiles + 1))
done <<< "$benchmarks"

extra_date=`date +%H-%M-%S`

echo

if [ $numFiles -gt 1 ]; then
    # initially, no hours/minutes/seconds
    merged_file=${date}-${vss_type}-vss-${t1min}-${t2max}-${machine}.csv
    new_logfile=${date}-${vss_type}-vss-${t1min}-${t2max}-${machine}.log
    while [ -f "$merged_file" -o -f "$new_logfile" ]; do
        # but if file name clashes, add hours/minutes/seconds
        extra_date=`date +%H-%M-%S`
        merged_file=${date}-${extra_date}-${vss_type}-vss-${t1min}-${t2max}-${machine}.csv
        new_logfile=${date}-${extra_date}-${vss_type}-vss-${t1min}-${t2max}-${machine}.log
    done

    mv "$logfile" "$new_logfile"

    cat $files >$merged_file
    rm $files

    echo "Merged files: "
    echo
    for f in $files; do echo "  $f"; done
    echo
    echo "...into: "
    echo
    echo "  $merged_file"
    echo
fi
