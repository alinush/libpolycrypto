#!/bin/bash
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

if [ $# -lt 1 ]; then
    echo "Usage: $0 <file1> [<file2> ...]"
    exit 1
fi

$scriptdir/cols.sh "$@" 1,2,3,4,5,6,7,8,9,15,16,17,18,19
