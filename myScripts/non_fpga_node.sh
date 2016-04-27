#! /bin/bash
#
. ./fpga.cfg
pattern=$PATTERN
job_num=$JOB_NUM
mean=$MEAN
node=`hostname`
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

cmd="./set_job.py $job_num $mean $node" 
exe "$cmd"
cmd="./execute_job.sh $node > ../logInfo/joblog-$node-${mean}-${pattern}-${datetime}.log &" 
exe "$cmd"
exit 0
