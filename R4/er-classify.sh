#!/bin/bash

set -ue -o pipefail

BER_BIN=$EVENTRACER_DIR/bin/eventracer/webapp/raceanalyzer

# INPUT HANDLING

if (( ! $# > 0 )); then
    echo "Usage: <base dir> <port>"
    echo "Generates EventRacer classifier data for all races in <base dir>"
    exit 1
fi

OUTDIR=$1
PORT=$2

if [ -f $OUTDIR/schedule.data ]; then
    $BER_BIN $OUTDIR/ER_actionlog -port $PORT &
    sleep 5
    wget --quiet --output-document $OUTDIR/varlist http://localhost:$PORT/varlist
    wget --quiet --output-document $OUTDIR/memlist http://localhost:$PORT/varlist?filter_level=1

    pkill -P $$
else
    exit 1
fi

