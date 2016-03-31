#!/bin/bash

RECORD=$WEBERA_DIR/R4/record.sh
MC=$WEBERA_DIR/R4/model-check.sh

mkdir -p output

OUTDIR=~/r4tmp/$RANDOM
while [ -f $OUTDIR ]; do
    OUTDIR=~/r4tmp/$RANDOM
done

OUTRUNNER=$OUTDIR/runner

if (( ! $# > 0 )); then
    echo "Usage: <website URL>"  
    echo "Outputs result to output/<website dir>"
    exit 1
fi

PROTOCOL=http

if (( $# == 2 )); then
    PROTOCOL=$2
fi

SITE=$1

echo "Running "  $PROTOCOL $SITE " @ " $OUTDIR
rm -rf $OUTDIR
mkdir -p $OUTRUNNER

echo "Running CMD: $RECORD $SITE $OUTDIR --verbose --auto >> $OUTRUNNER/record.log 2>> $OUTRUNNER/record.log"
$RECORD $SITE $OUTDIR --verbose --auto >> $OUTRUNNER/record.log 2>> $OUTRUNNER/record.log
if [[ $? == 0 ]] ; then
    echo "Running CMD: $MC $SITE $OUTDIR --verbose --auto --depth 1  >> $OUTRUNNER/mc.log 2>> $OUTRUNNER/mc.log"
    $MC $SITE $OUTDIR --verbose --auto --depth 1  >> $OUTRUNNER/mc.log 2>> $OUTRUNNER/mc.log
    if [[ ! $? == 0 ]] ; then
        echo "model-checker errord out, see $OUTRUNNER/mc.log"
    elif [ ! -d $OUTDIR/base ]; then
        echo "Error: Repeating the initial recording failed"
    fi

else
    echo "Initial recording errord out, see $OUTRUNNER/record.log"
fi

ID=${SITE//[:\/\.]/\-}

rm -rf output/$ID
mv $OUTDIR output/$ID


