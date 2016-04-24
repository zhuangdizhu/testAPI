#define OS_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <Python.h>
#include "tcp_transfer.h"
#include "rdma_server.h"
#include "fpga-sim/driver/fpga-libacc.h"


static PyObject* start_service(PyObject *self, PyObject *args)
{
    PyObject * ret;
    const char *job_id, *status, *section_id, *c_real_in_buf_size, *c_in_buf_size, *c_out_buf_size, *c_acc_name, *scheduler_host, *scheduler_port;

    if (!PyArg_ParseTuple(args, "sssssssss", &job_id, &status, &section_id, &c_real_in_buf_size, &c_in_buf_size, &c_out_buf_size, &c_acc_name, &scheduler_host, &scheduler_port)){
    	return NULL;
    }

    struct server_param_t server_param;
    memset((void *) &server_param, 0, sizeof(server_param));

    strcpy(server_param.job_id, job_id);
    strcpy(server_param.section_id, section_id); 
    server_param.real_in_buf_size = atoi(c_real_in_buf_size);
    server_param.in_buf_size = atoi(c_in_buf_size);
    server_param.out_buf_size = atoi(c_out_buf_size);
    strcpy(server_param.acc_name, c_acc_name);
    strcpy(server_param.scheduler_host, scheduler_host);
    strcpy(server_param.scheduler_port, scheduler_port);


    if (atoi(status) == 2)
        //open an RDMA server and local acc_slot;
        rdma_server_open((void *)&server_param);
    else if (atoi(status) == 0){ 
        //open a socket server and local acc_slot;
        int tcp_status = socket_server_open((void *) &server_param);
        if(tcp_status != 0){
            sprintf(server_param.status,"%d",tcp_status); 
        }
    }

    //return port number and open_status(success or failure)back to client.
    ret = Py_BuildValue("{s:s, s:s, s:s}", "ip", server_param.ipaddr, "port", server_param.port, "ifuse", server_param.status);
    printf("ip = %s, port =%s, ifuse = %s\n", server_param.ipaddr, server_param.port, server_param.status);
    return ret;
}

static PyMethodDef DemoMethods[] = {
    {"start_service",  start_service, METH_VARARGS,
     "acc_monitor function"},
    {NULL, NULL, 0, NULL}
};

static PyObject *DemoError;
static char name[100];

PyMODINIT_FUNC initacc_monitor(void)
{
    PyObject *m;

    m = Py_InitModule("acc_monitor", DemoMethods);

    if (m == NULL)
        return;

    strcpy(name, "acc_monitor.error");
    DemoError = PyErr_NewException(name, NULL, NULL);
    Py_INCREF(DemoError);
    strcpy(name, "error");
    PyModule_AddObject(m, name, DemoError);
}


int main(int argc, char *argv[])
{
    Py_SetProgramName(argv[0]);
    Py_Initialize();
    initacc_monitor();
    return 1;
}
