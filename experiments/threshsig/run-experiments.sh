#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Runs the threshold signature benchmark for the specified scheme with thresholds f+1 = 2^k, k \\in {1, 2, 3, ..., 20} and n = 2f+1"
    echo
    echo "Usage: $0 <machine-type> <scheme(s)>"
    echo
    echo "OPTIONS:"
    echo "   <scheme(s)> can be either 'naive' or 'efficient'"
    exit 1
fi

date=`date +%Y-%m-%d` # %H-%M-%S if you want hours, minutes & seconds
machine=$1
scheme=$2
multicore=0

if [ $scheme = "naive" ]; then
    isEff=0
elif [ $scheme = "efficient" ]; then
    isEff=1
else
    echo "ERROR: <scheme> must be either 'naive' or 'efficient'"
    exit 1
fi


which BenchThresholdSig 2>&1 >/dev/null || { echo "ERROR: You did not set up the environment"; exit 1; }
# Format: <min-t> <max-t> <num-deal-iters> <scheme>
benchmarks="\
2       256     100 naive
512     4096    10 naive
8192    16384   10 naive
32768   32768   8  naive
65536   65536   4  naive
131072  131072  2  naive
262144  262144  1  naive
524288  524288  1  naive
1048576 1048576 1  naive
2       256     100 efficient
512     1048576 10 efficient
"

files=
numFiles=0
t1min=$((1048576*1024))
t2max=0
while read -r b; do
    t1=`echo $b | cut -d' ' -f 1`
    t2=`echo $b | cut -d' ' -f 2`
    numDealIters=`echo $b | cut -d' ' -f 3`
    sch=`echo $b | cut -d' ' -f 4`

    if [ "$sch" != "$scheme" ]; then
        continue
    fi

    echo "Running t \\in $t1 to $t2 $vss threshold sigs with $numDealIters iters"

    [ $t1 -lt $t1min ] && t1min=$t1
    [ $t2 -gt $t2max ] && t2max=$t2
    
    file=${date}-${sch}-threshsig-${t1}-${t2}-${machine}.csv
    logfile=${date}-${sch}-threshsig-${t1}-${t2}-${machine}.log
    echo "Logging to $logfile ..."
    if [ $multicore -eq 1 ]; then
        BenchThresholdSig $isEff $t1 $t2 $numDealIters $file 2>&1 >$logfile &
        sleep 1
    else
        BenchThresholdSig $isEff $t1 $t2 $numDealIters $file 2>&1 | tee -a $logfile
    fi

    files="$files $file"
    logfiles="$logfiles $logfile"
    numFiles=$(($numFiles + 1))
done <<< "$benchmarks"

if [ $multicore -eq 1 ]; then
    echo "Waiting for them to finish..."
    wait
fi

extra_date=`date +%H-%M-%S`

echo

if [ $numFiles -gt 1 ]; then
    # initially, no hours/minutes/seconds
    merged_file=${date}-${scheme}-threshsig-${t1min}-${t2max}-${machine}.csv
    new_logfile=${date}-${scheme}-threshsig-${t1min}-${t2max}-${machine}.log
    while [ -f "$merged_file" -o -f "$new_logfile" ]; do
        # but if file name clashes, add hours/minutes/seconds
        extra_date=`date +%H-%M-%S`
        merged_file=${date}-${extra_date}-${scheme}-threshsig-${t1min}-${t2max}-${machine}.csv
        new_logfile=${date}-${extra_date}-${scheme}-threshsig-${t1min}-${t2max}-${machine}.log
    done

    cat $logfiles >"$new_logfile"
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
