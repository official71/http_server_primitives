#!/usr/bin/env bash

N=10000
declare -a CS=(1 10 100 500)
# declare -a CS=(500)
OUTFILE="output.log"
TMPFILE="tmpfilexx"
TIMESTAMP=`date +"%m/%d %H:%M:%S"`

echo -e "\n========================\n" >> $OUTFILE
echo $TIMESTAMP >> $OUTFILE

for C in "${CS[@]}"
do
    cmd="ab -n $N -c $C http://127.0.0.1:8080/ "
    echo "command: " $cmd
    eval $cmd 2>/dev/null > $TMPFILE

    # save to logs
    echo -e "\n.................." >> $OUTFILE
    echo $cmd >> $OUTFILE
    cat $TMPFILE >> $OUTFILE

    # parse results
    grep "Failed request" $TMPFILE
    grep "Time per request" $TMPFILE
    grep "Requests per second" $TMPFILE
    echo ""

    sleep 5
done
