#!/bin/bash

RECORD_BIN=$WEBERA_DIR/R4/clients/Record/bin/record
REPLAY_BIN=$WEBERA_DIR/R4/clients/Replay/bin/replay
ER_BIN=$EVENTRACER_DIR/bin/eventracer/webera/run_schedules
BER_BIN=$EVENTRACER_DIR/bin/eventracer/webera/webera

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
AUTO=1

if [[ $AUTO -eq 1 ]]; then
    AUTOCMD="-hidewindow -autoexplore -autoexplore-timeout 15"
    AUTOCMD2="-hidewindow -verbose"
else
    AUTOCMD="-ignore-mouse-move"
    AUTOCMD2="-verbose"
fi

for site in $SITES
do
    echo "Running "  $PROTOCOL $site " @ " $OUTDIR

    rm -rf $OUTDIR
    mkdir -p $OUTRECORD
    mkdir -p $OUTRUNNER

    if [[ $VERBOSE -eq 1 ]]; then
        echo "> $RECORD_BIN -out_dir $OUTRECORD $AUTOCMD $PROTOCOL://$site > $OUTRECORD/out.log 2> $OUTRECORD/out.log"
    fi

    $RECORD_BIN -out_dir $OUTRECORD $AUTOCMD $PROTOCOL://$site > $OUTRECORD/out.log 2> $OUTRECORD/out.log

    if [[ $? == 0 ]] ; then

        if [[ $VERBOSE -eq 1 ]]; then
            echo "> /usr/bin/time -p $ER_BIN -in_dir=$OUTRECORD/ -in_schedule_file=$OUTRECORD/schedule.data -tmp_new_schedule_file=$OUTDIR/new_schedule.data -out_dir=$OUTDIR -tmp_error_log=$OUTDIR/out.errors.log -tmp_network_log=$OUTDIR/out.log.network.data  -tmp_time_log=$OUTDIR/out.log.time.data  -tmp_random_log=$OUTDIR/out.log.random.data  -tmp_status_log=$OUTDIR/out.status.data -tmp_png_file=$OUTDIR/out.screenshot.png -tmp_schedule_file=$OUTDIR/out.schedule.data -tmp_stdout=$OUTDIR/stdout.txt -tmp_er_log_file=$OUTDIR/out.ER_actionlog --site=$PROTOCOL://$site --replay_command=\"$REPLAY_BIN $AUTOCMD2 -out_dir $OUTDIR -timeout 60 -in_dir %s/ \"%s\" %s\" &> $OUTRUNNER/stdout.txt"
        fi

        /usr/bin/time -p $ER_BIN -in_dir=$OUTRECORD/ -in_schedule_file=$OUTRECORD/schedule.data -tmp_new_schedule_file=$OUTDIR/new_schedule.data -out_dir=$OUTDIR -tmp_error_log=$OUTDIR/out.errors.log -tmp_network_log=$OUTDIR/out.log.network.data -tmp_time_log=$OUTDIR/out.log.time.data -tmp_random_log=$OUTDIR/out.log.random.data -tmp_status_log=$OUTDIR/out.status.data -tmp_png_file=$OUTDIR/out.screenshot.png -tmp_schedule_file=$OUTDIR/out.schedule.data -tmp_stdout=$OUTDIR/stdout.txt -tmp_er_log_file=$OUTDIR/out.ER_actionlog --site=$PROTOCOL://$site --replay_command="$REPLAY_BIN $AUTOCMD2 -out_dir $OUTDIR -timeout 60 -in_dir %s/ \"%s\" %s" &> $OUTRUNNER/stdout.txt

        if [[ ! $? == 0 ]] ; then
            echo "Search errored out, see $OUTRUNNER/stdout.txt"
            continue
        fi

        if [ ! -d $OUTDIR/base ]; then
            echo "Error: Repeating the initial recording failed"
            continue
        fi

    else
        echo "Initial recording failed, see $OUTRECORD/out.log"
        continue
    fi

    ID=${site//[:\/\.]/\-}

    rm -rf output/$ID
    mv $OUTDIR output/$ID

done
