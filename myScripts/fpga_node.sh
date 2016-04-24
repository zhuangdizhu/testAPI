#! /bin/bash
# run on fpga node
#
. ./fpga.cfg

job_num=$JOB_NUM
mean=$MEAN
deamon_port=$DEAMON_PORT
node=`hostname`

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

cmd="./set_job.py $job_num $mean $node"
exe "$cmd"
#cmd="../deamon.py $deamon_port &"
#exe "$cmd"
cmd="./execute_job.sh $node &" 
exe "$cmd"
exit 0
