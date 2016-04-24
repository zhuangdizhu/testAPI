#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/10/30 18:28:51
#   Desc    :
#
import sys
import argparse
import uuid
import datetime
import time
import socket
import json #for serialization/de-serialization
import acc_monitor #for open a socket communicating with remote server
from SocketServer import (BaseRequestHandler as BRH, ThreadingMixIn as TMI, TCPServer)
print sys.argv

global BUFFER_SIZE
BUFFER_SIZE = 1024
global scheduler_host, scheduler_port


class TCPServer(TCPServer):
    allow_reuse_address = True


class Server(TMI, TCPServer):
    pass

class MyRequestHandler(BRH):
    def handle(self):
        try:
            print "...connected from:", self.client_address
            raw_data = self.request.recv(BUFFER_SIZE)
            scheduler_context = json.loads(raw_data)
            job_id = scheduler_context["job_id"]
            status = scheduler_context["status"]
            section_id = scheduler_context["section_id"]
            real_in_buf_size = scheduler_context["real_in_buf_size"]
            in_buf_size = scheduler_context["in_buf_size"]
            out_buf_size = scheduler_context["out_buf_size"]
            acc_name = scheduler_context["acc_name"]
            #for i,j in scheduler_context.items():
            #    print i,j
            ret_context = acc_monitor.start_service(job_id, status, section_id,
                                                    real_in_buf_size, in_buf_size,
                                                    out_buf_size, acc_name,
                                                    str(scheduler_host), str(scheduler_port))
            #for i,j in ret_context.items():
            #    print i,j
            send_data = json.dumps(ret_context)
            self.request.sendall(send_data)
        except KeyboardInterrupt:
            print "Existing..."


def run_deamon(daemon_port, s_host, s_port):

    global scheduler_host
    scheduler_host = s_host
    global scheduler_port
    scheduler_port = s_port
    daemon_host = ''
    daemon_address  = (daemon_host, int(daemon_port))
    server = Server(daemon_address, MyRequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print 'Existing ...'
        server.server_close()

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print "Usage: ", sys.argv[0], "<daemon port> <scheduler_host>, <scheduler_port>"
        print "Example: ./deamon.py 5000 tian01 9000"
    else:
        print "Running deamon process...."
        run_deamon(sys.argv[1], sys.argv[2], sys.argv[3])


