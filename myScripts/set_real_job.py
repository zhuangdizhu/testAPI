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
        self.poisson_interval_mean = mean
        self.exponential_interval_mean = mean
        try:
            os.remove("job_"+node+".txt")
        except OSError:
            pass
        self.target = open("job_"+node+".txt",'a')

    def poisson_generate_job(self, job_num):
        job_arrival_time_array = numpy.random.poisson(self.poisson_interval_mean, job_num)
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(256,1024)
            out_buf_size = in_buf_size
            arrival_time = job_arrival_time_array[i]/10.0
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check 'job_%s.txt'" %node

    def exponential_generate_job(self, job_num):
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(256,1024)
            out_buf_size = in_buf_size
            arrival_time = 1000000*random.expovariate(self.exponential_interval_mean)
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check 'job_%s.txt'" %node

    def generate_job(self, job_num):
        for i in range(job_num):
            acc_name = random.sample(self.acc_type_list,1)[0]


if __name__ == "__main__":
	if len(sys.argv) < 4:
		sys.exit("Usage: "+sys.argv[0]+" <job_num(integer)> <arrival_interval_mean(integer) <node>")
		sys.exit(1)
	job_num = int(sys.argv[1])
	mean = int(sys.argv[2])
	node = sys.argv[3]
	job_initiator = JobInitiator(mean, node)
	job_initiator.exponential_generate_job(job_num)
	#job_initiator.generate_job(job_num)

