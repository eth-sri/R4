#!/bin/bash

BER_BIN=$EVENTRACER_DIR/bin/eventracer/webapp/raceanalyzer

# INPUT HANDLING

if (( ! $# > 0 )); then
    echo "Usage: <base dir> <port> <event action id...>"
    echo "Generates EventRacer code data for <event action id> in <base dir>"
    exit 1
fi

OUTDIR=$1
PORT=$2
shift
shift

$BER_BIN -drop_node_analysis=false -port=$PORT $OUTDIR/ER_actionlog &
sleep 2

while [[ $# > 0 ]]
do
    EVENT_ACTION_ID=$1
    wget --quiet --output-document $OUTDIR/varlist-$EVENT_ACTION_ID.code http://localhost:$PORT/code?focus=$EVENT_ACTION_ID
    shift
done

pkill -P $$
