#!/bin/bash
. ./fpga.cfg

scheduler_node=$SCHEDULER_NODE
scheduler_port=$SCHEDULER_PORT

node=$1

FILE="../jobInfo/job_${node}.txt"
if [ ! -f $FILE ]; then
	echo "$FILE: does not exists"
	exit 1
elif [ ! -r $FILE ]; then
	echo "$FILE: can not be read"
	exit 2
fi

BAKIFS=$IFS
IFS=$(echo -en "\n\b")
exec 3<&0
exec 0<"$FILE"

while read -r line
do
	#processLine $line 
  	arg1=$(echo $line | awk '{ print $1 }')
  	arg2=$(echo $line | awk '{ print $2 }')
  	arg3=$(echo $line | awk '{ print $3 }')
    ../test_bench.sh $arg1 $arg2 $scheduler_node $scheduler_port &
	sleep $arg3
done 
exec 0<&3
IFS=$BAKIFS
