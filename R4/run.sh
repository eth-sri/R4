#!/bin/bash

RECORD_BIN=$WEBERA_DIR/R5/clients/Record/bin/record
REPLAY_BIN=$WEBERA_DIR/R5/clients/Replay/bin/replay
LD_PATH=$WEBERA_DIR/WebKitBuild/Release/lib
ER_BIN=$EVENTRACER_DIR/bin/eventracer/webera/run_schedules
BER_BIN=$EVENTRACER_DIR/bin/eventracer/webera/webera

export LD_LIBRARY_PATH=$LD_PATH

mkdir -p output

OUTDIR=/tmp/$RANDOM
while [ -f $OUTDIR ]; do
    OUTDIR=/tmp/$RANDOM
done

OUTRECORD=$OUTDIR/record
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

SITES=$1

if [ -f $1 ]; then
    SITES=`cat $1`
fi

VERBOSE=1

for site in $SITES
do
    echo "Running "  $PROTOCOL $site " @ " $OUTDIR

    rm -rf $OUTDIR
    mkdir -p $OUTRECORD
    mkdir -p $OUTRUNNER

    if [[ $VERBOSE -eq 1 ]]; then
        echo "> $RECORD_BIN -out_dir $OUTRECORD -autoexplore -autoexplore-timeout 10 $PROTOCOL://$site > $OUTRECORD/out.log 2> $OUTRECORD/out.log"
    fi

    $RECORD_BIN -out_dir $OUTRECORD -autoexplore -autoexplore-timeout 10 $PROTOCOL://$site > $OUTRECORD/out.log 2> $OUTRECORD/out.log

    

    if [[ $? == 0 ]] ; then

        if [[ $VERBOSE -eq 1 ]]; then
            echo "> /usr/bin/time -p $ER_BIN $OUTRECORD/ER_actionlog -in_schedule_file=$OUTRECORD/schedule.data -out_dir=$OUTDIR -tmp_error_log=$OUTDIR/out.errors.log -tmp_png_file=$OUTDIR/out.screenshot.png -tmp_schedule_file=$OUTDIR/out.schedule.data -tmp_stdout=$OUTDIR/stdout.txt --site=$PROTOCOL://$site --replay_command=\"LD_LIBRARY_PATH=$LD_PATH $REPLAY_BIN -out_dir $OUTDIR -timeout 60 \"%s\" %s $OUTRECORD/log.network.data $OUTRECORD/log.random.data $OUTRECORD/log.time.data\" > $OUTRUNNER/stdout.txt 2> $OUTRUNNER/stdout.txt"
        fi

        /usr/bin/time -p $ER_BIN $OUTRECORD/ER_actionlog -in_schedule_file=$OUTRECORD/schedule.data -out_dir=$OUTDIR -tmp_error_log=$OUTDIR/out.errors.log -tmp_png_file=$OUTDIR/out.screenshot.png -tmp_schedule_file=$OUTDIR/new_schedule.data -tmp_stdout=$OUTDIR/stdout.txt --site=$PROTOCOL://$site --replay_command="LD_LIBRARY_PATH=$LD_PATH $REPLAY_BIN -out_dir $OUTDIR -timeout 60 \"%s\" %s $OUTRECORD/log.network.data $OUTRECORD/log.random.data $OUTRECORD/log.time.data" > $OUTRUNNER/stdout.txt 2> $OUTRUNNER/stdout.txt

        if [[ ! $? == 0 ]] ; then
            cat $OUTRUNNER/stdout.txt
            exit 1
        fi

        if [ ! -d $OUTDIR/base ]; then
            echo "Error: Repeating the initial recording failed"
            exit 1
        fi

        if [ -f $OUTRECORD/schedule.data ]; then
                killall --quiet -w webera
                $BER_BIN $OUTRECORD/ER_actionlog -in_schedule_file=$OUTRECORD/schedule.data > /dev/null &
                sleep 2
                wget --quiet -P $OUTRECORD http://localhost:8000/varlist
                killall --quiet -w webera
        fi

    else
        cat $OUTRECORD/out.log
        exit 1
    fi

    ID=${site//[:\/\.]/\-}

    rm -rf output/$ID
    mv $OUTDIR output/$ID

done
