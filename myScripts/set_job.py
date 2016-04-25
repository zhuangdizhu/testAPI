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
import numpy
import random
import os

class JobInitiator(object):
    def __init__(self, mean, node):
        self.acc_type_list = ['AES','DTW','EC','FFT','SHA']
        self.exp_lambda = 1/mean
        try:
            os.remove("../jobInfo/job_"+node+".txt")
        except OSError:
            pass
        self.target = open("../jobInfo/job_"+node+".txt",'a')

    def generate_job(self, job_num):
        for i in range(job_num):
            acc_name = random.sample(self.acc_type_list,1)[0]
            #in_buf_size = mean
            in_buf_size = random.expovariate(self.exp_lambda)   #in MBytes
            in_buf_size = int(in_buf_size*256)                       #in 4K Bytes


            out_buf_size = in_buf_size
            arrival_time = 1
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check '../jobInfo/job_%s.txt'" %node


if __name__ == "__main__":
	if len(sys.argv) < 4:
		sys.exit("Usage:    "+sys.argv[0]+" <job_num> <job_size_mean/MBytes> <node_name/hostname>")
		sys.exit("Example:  "+sys.argv[0]+" 10 400 tian01")
		sys.exit(1)
	job_num = int(sys.argv[1])
	mean = int(sys.argv[2])
	node = sys.argv[3]
	job_initiator = JobInitiator(mean, node)
	job_initiator.generate_job(job_num)

