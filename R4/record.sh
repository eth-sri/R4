#!/bin/bash

set -ue -o pipefail

# INPUT HANDLING

if (( ! $# > 0 )); then
    echo "Usage: <website URL> <base dir> [--verbose] [--auto] [--cookie key=value]* [--move-mouse]"  
    echo "Outputs result of recording to <base dir>/record"
    exit 1
fi

URL=$1
OUTDIR=$2

shift
shift

PROTOCOL=http
VERBOSE=0
AUTO=0
COOKIESCMD=""
MOVEMOUSE=0

while [[ $# > 0 ]]
do

case $1 in
    --auto)
        AUTO=1
        shift
    ;;
    --move-mouse)
        MOVEMOUSE=1
        shift
    ;;
    --verbose)
        VERBOSE=1
        shift
    ;;
    --cookie)
        shift
        COOKIESCMD="$COOKIESCMD -cookie $1"
        shift
    ;;
    *)
        echo "Unknown option $1"
        exit 1
    ;;
esac
done

# DO SOMETHING

mkdir -p $OUTDIR

RECORD_BIN=$WEBERA_DIR/R4/clients/Record/bin/record
OUTRECORD=$OUTDIR/record

if [[ $VERBOSE -eq 1 ]]; then
    VERBOSECMD="-verbose"
else
    VERBOSECMD=""
fi

if [[ $AUTO -eq 1 ]]; then
    AUTOCMD="-hidewindow -autoexplore -autoexplore-timeout 10"
else
    if [[ $MOVEMOUSE -eq 1 ]]; then
        AUTOCMD=""
    else
        AUTOCMD="-ignore-mouse-move"
    fi
fi

echo "Running "  $PROTOCOL $URL " @ " $OUTDIR
mkdir -p $OUTRECORD

CMD="$RECORD_BIN -out_dir $OUTRECORD $AUTOCMD $VERBOSECMD $COOKIESCMD $PROTOCOL://$URL"

if [[ $VERBOSE -eq 1 ]]; then
    echo "> $CMD"
    $CMD 2>&1 | tee $OUTRECORD/out.log
else
    $CMD &> $OUTRECORD/out.log
fi

