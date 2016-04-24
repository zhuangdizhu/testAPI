#!/bin/bash
#
# FPGA test script
# by zzd and Lei
# for performance testing
#
# prerequisite:
# 1. modify fpga_node.txt with node information
# 2. modify fpga.cfg for your environment
#
# usage: ./test_api.sh [start|stop]
#

# read node list from fpga_node.txt

# show usage info

path="/home/tian/zdzhu/testAPI/"
#path="/Users/zhuzhuangdi/Documents/mytests/testAPI/"

usage() {
	echo "Usage: test_fpga.sh [start|status|stop]"
}

read_conf() {
	echo "Reading config ..." >&2
	. ./fpga.cfg
	if [ -z "$PATTERN" ]; then
		pattern="Local"
	else 
		pattern=$PATTERN
	fi
	if [ -z "$MEAN" ]; then
		mean="400"
	else 
		mean=$MEAN
	fi
	if [ -z "$JOB_NUM" ]; then
		job_num="10"
	else 
		job_num=$JOB_NUM
	fi
	if [ -z "$FPGA_NODES" ]; then
		fpga_nodes="tian02,tian03,tian04"
	else 
		fpga_nodes=$FPGA_NODES
	fi
	if [ -z "$OTHER_NODES" ]; then
		other_nodes="tian05,tian06"
	else 
		other_nodes=$OTHER_NODES
	fi
	if [ -z "$SCHEDULE_NODE" ]; then
        scheduler_node="tian01"
	else 
        scheduler_node=$SCHEDULE_NODE
	fi
	if [ -z "$SCHEDULE_PORT" ]; then
        scheduler_port="9000"
	else 
        scheduler_node=$SCHEDULE_PORT
	fi
	if [ -z "$DEAMON_PORT" ]; then
        deamon_port="5000"
	else 
        deamon_node=$DEAMON_PORT
	fi

	echo "  Config: $pattern, $mean, $job_num"
    echo "  Config: $fpga_nodes, $other_nodes"
    echo "  Config: $scheduler_node, $scheduler_port, $deamon_port"
}

# start scheduler on SCHEDULE_NODE, 
# start server and tests based on PATTERN
test_start() {
	# run scheduler on current node 
	run_scheduler $pattern $mean
}

# check scheduler and server status
test_status() {
	ps aux | egrep [f]pga_scheduler
	pdsh -w $fpga_nodes 'ps aux | egrep "[d]eamon"'
	pdsh -w $allnodes 'ps aux | egrep "[e]xecute_job"'
}

# kill scheduler on SCHEDULE_NODE, 
# kill server and tests based on all nodes 
test_stop() {
	pkill -9 -f fpga_scheduler
	pdsh -w $allnodes 'pkill -9 -f execute_job'
	pdsh -w $fpga_nodes 'pkill -9 -f deamon'
}

# print and execute command string
# comment out eval while debugging
exe() {
	if [[ "$#" -eq "1" ]]; then
		echo "    CMD: $1"
		eval $1
	else
		echo "  error in exe call"
		exit -1
	fi
}

run_scheduler() {
	pattern=$1
	mean=$2
	
	echo "  scheduler is using FIFO algorithm, mean = ${mean}, pattern = ${pattern}"
	datetime=`date +"%Y%m%d-%H%M"`
    if [[ $pattern = "Local" ]]; then
	    cmd="../fpga_scheduler.py $scheduler_port Local ../fpga_node.txt > ../logInfo/$pattern-mean$mean-${algorithm}-${datetime}.log &"
	    exe "$cmd"
    else
	    cmd="../fpga_scheduler.py $scheduler_port TCP ../fpga_node.txt > ../logInfo/$pattern-mean$mean-${algorithm}-${datetime}.log &"
	    exe "$cmd"
    fi
	
    #Local mode, ONLY jobs from FPGA-equipped nodes will be issued.
	if [[ $pattern = "Local" ]]; then
		cmd="pdsh -w $fpga_nodes \"cd $path; cd myScripts/; ./fpga_node.sh > ../logInfo/fpganode-${pattern}.log &\""
		echo "$cmd"; eval "$cmd"

	#Remote Mode, ONLY jobs from non-FPGA-equipped node will be issued
	elif [[ $pattern = "Remote" ]]; then
		cmd="pdsh -w $fpga_nodes \"cd $path; ./deamon.py $deamon_port $scheduler_node $scheduler_port > ../logInfo/fpganode-${pattern}.log &\""
		echo "$cmd"; eval "$cmd"
		cmd="pdsh -w $other_nodes \"cd $path; cd myScripts; ./non_fpga_node.sh > ../logInfo/othernode-${pattern}.log &\""
		echo "$cmd"; eval "$cmd"
		
    #Global mode. BOTH jobs from FPGA-equipped and non-FPGA-equipped nodes will be issued.
	elif [[ $pattern = "Global" ]]; then
		cmd="pdsh -w $fpga_nodes \"cd $path; cd myScripts/; ./fpga_node.sh > ../logInfo/fpganode-${pattern}.log &\""
		echo "$cmd"; eval "$cmd"
		cmd="pdsh -w $other_nodes \"cd $path; cd myScripts/; ./non_fpga_node.sh > ../logInfo/othernode-${pattern}.log &\""
		echo "$cmd"; eval "$cmd"
	fi
}

read_conf
allnodes="$fpga_nodes,$other_nodes"

if [[ "$#" -eq "1" ]]; then
	if  [[ "$1" = "start" ]]; then
		echo "Starting test_fpga:"
		test_start
		echo "  test_fpga started"
	elif [[ "$1" = "status" ]]; then
		echo "test_fpga status:"
		test_status
	elif [[ "$1" = "stop" ]]; then
		echo "Stopping test_fpga:"
		test_stop
		echo "  test_fpga stopped"
	else 
		usage
	fi
else
	usage
fi

exit
