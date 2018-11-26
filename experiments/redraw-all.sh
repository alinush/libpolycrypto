#!/bin/sh
set -e

scriptdir=$(cd $(dirname $0); pwd -P)

#echo "Script dir: $scriptdir"

(cd $scriptdir/threshsig/; pwd; ./redraw.sh; )
(cd $scriptdir/dkg/; ./redraw.sh )
(cd $scriptdir/dkg-bw/; ./redraw.sh )
(cd $scriptdir/vss/; ./redraw.sh )

# DKG dealing times are basically the same as eVSS dealing times
$scriptdir/redraw-vss-and-dkg-deal.sh
