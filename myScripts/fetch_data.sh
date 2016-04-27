#!/bin/bash

usage() {
    echo "Example: fetch_data.sh 400 Remote "
}

if [[ "$#" -eq "2" ]]; then
    mean=$1
    pattern=$2
    node=`hostname`
    node="tian03"
    #echo $mean $pattern $node
    t_file="../logInfo/fetched-${node}-${mean}-${pattern}"
    s_dir="../logInfo"

    tmpRet=`ls $s_dir|grep $pattern |grep $mean|grep log`
    fileArray=($tmpRet)
    cnt=0
    for s_file in ${fileArray[@]}
    do
        cnt=$(($cnt+1))
        echo $cnt
        #echo $s_file
        openLog=`grep "OPEN" $s_dir/${s_file} | awk '{print $5}'`
        exeLog=`grep "EXE" $s_dir/${s_file} |awk '{print $7}'`
        closeLog=`grep "CLOSE" $s_dir/${s_file} |awk '{print $9}'`
        
        openArray=($openLog)
        exeArray=($exeLog)
        closeArray=($closeLog)

        len=${#openArray[@]}
        len=$(($len-1))
        #echo $len
        echo "${openArray[0]},${exeArray[0]},${closeArray[0]}" > $t_file.$cnt.csv 
        for ((i=1; i<=$len; i++));do
            echo "${openArray[$i]},${exeArray[$i]},${closeArray[$i]}" >> $t_file.$cnt.csv 

        done 
    done
else
    usage
fi

exit
