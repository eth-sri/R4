#!/bin/bash

BER_BIN=$EVENTRACER_DIR/bin/eventracer/webapp/raceanalyzer

# INPUT HANDLING

if (( ! $# > 0 )); then
    echo "Usage: <base dir> <port>"
    echo "Generates EventRacer classifier data for all races in <base dir>"
    exit 1
fi

if [ -z "$ER_CLASSIFY_EXTRA" ]; then
    ER_CLASSIFY_EXTRA=""
fi

OUTDIR=$1
PORT=$2

if [ -f $OUTDIR/schedule.data ]; then
    $BER_BIN $ER_CLASSIFY_EXTRA -drop_node_analysis=false -port=$PORT $OUTDIR/ER_actionlog &
    sleep 5
    wget --quiet --output-document $OUTDIR/varlist http://localhost:$PORT/varlist
    wget --quiet --output-document $OUTDIR/memlist http://localhost:$PORT/varlist?filter_level=1

    pkill -P $$
else
    exit 1
fi

