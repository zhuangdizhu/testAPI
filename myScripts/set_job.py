#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/09/28 15:05:07
#   Desc    :
#
from __future__ import division
import sys
import random
import os

class JobInitiator(object):
    def __init__(self, mean, node, interval):
        self.acc_type_list = ['AES']
        self.exp_lambda = 1/mean
        try:
            os.remove("../jobInfo/job_"+node+"_"+str(mean)+".txt")
        except OSError:
            pass
        self.target = open("../jobInfo/job_"+node+"_"+str(mean)+".txt",'a')

    def generate_job(self, job_num):
        for i in range(job_num):
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = mean
            #in_buf_size = random.expovariate(self.exp_lambda)   #in MBytes

            in_buf_size = int(in_buf_size*256)                       #in 4K Bytes
            out_buf_size = in_buf_size

            arrival_time = interval
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check '../jobInfo/job_%s_%r.txt'" %(node,mean)


if __name__ == "__main__":
	if len(sys.argv) < 5:
		sys.exit("Usage:    "+sys.argv[0]+" <job_num> <job_size_mean/MBytes> <node_name/hostname>")
		sys.exit("Example:  "+sys.argv[0]+" 10 400 tian01")
		sys.exit(1)
	job_num = int(sys.argv[1])
	mean = int(sys.argv[2])
	node = sys.argv[3]
	interval = int(sys.argv[4])
	job_initiator = JobInitiator(mean, node, interval)
	job_initiator.generate_job(job_num)

