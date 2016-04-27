#! /bin/bash
# run on fpga node
#
. ./fpga.cfg

job_num=$JOB_NUM
mean=$MEAN
deamon_port=$DEAMON_PORT
node=`hostname`
scheduler_node=$SCHEDULER_NODE
scheduler_port=$SCHEDULER_PORT
datetime="$(date +'%m-%d-%H-%M')"

# print and execute command string
# comment out eval while debugging
exe() {
	if [[ "$#" -eq "1" ]]; then
		echo "  CMD: $1"
		eval $1
	else
		echo "  error in exe call"
		exit -1
	fi
}

echo "$deamon_port $scheduler_node $scheduler_port"
cmd="../deamon.py $deamon_port $scheduler_node $scheduler_port > ../logInfo/fpga-deamon.log &"
exe "$cmd"
exit 0
