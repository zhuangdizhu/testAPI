#include "fpga-sim/driver/fpga-libacc.h"


struct acc_context_t {
    char acc_name [16];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    unsigned int real_in_buf_size;
    unsigned int real_out_buf_size;
    void * in_buf;
    void ** result_buf;
    struct acc_handler_t acc_handler;
    void * tcp_context;
    void * rdma_context;
    void * scheduler_context;
};

struct scheduler_context_t {
    char scheduler_host[16];
    char server_host[16];
    int scheduler_port;
    int server_port;
    int to_scheduler_fd;

    void * scheduler_addr;
    void * to_scheduler_addr;

    char job_id[16];
	char section_id[16];/*this indicates which FPGA slot will be used*/
	char status[16]; /* this indicates the job is local, or remote TCP, or remote RDMA*/

    /*for throttle socket*/
    double max_bps;

    /* for debug only */
    long open_time;
    long execution_time;
    long close_time;
    long total_time;
    /* for debug only */
};

/*for debug only. this struct will send to scheduler when a job finishes, and the content will be printed out on the scheduler side*/
struct debug_context_t{
    char status[16];
    char job_id[16];
    char open_time[16];
    char execution_time[16];
    char close_time[16];
    char total_time[16];
    char backup01[16];
};

void * fpga_acc_open(
        struct acc_context_t * acc_context, 
        char * acc_name, 
        unsigned int in_buf_size, 
        unsigned int out_buf_size, 
        char * scheduler_host, 
        int scheduler_port, 
        char *uuid, 
        char *dest_node);

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf);

void fpga_acc_close(struct acc_context_t * acc_context);

