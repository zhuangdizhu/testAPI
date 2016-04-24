#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "acc_servicelayer.h"


int main(int argc, char **argv)
{	
	char *acc_name, *host;
	struct timeval t1, t2, dt;
	void * in_buffer;
    	void *result_buf;
	unsigned long ret, in_buf_size, out_buf_size, chunk_size, sent_block_size, left_block_size;
    unsigned long open_sec, exe_sec, close_sec;
	char param[16];
	int port, count;
	param[0] = 0x24;

	if (argc != 5) {
		printf("Usage : ./test_bench <acc_name> <workload> <scheduler host> <scheduler port>\n");
		printf("\tworkload must be a mutiple of 4 K bytes.\n");
		printf("\tExample : ./test_bench AES 1 localhost 9000(means executing AES with workloads of 1 * 4K bytes.\n");
		return -1;
   	}
	struct acc_context_t my_context;

	acc_name = argv[1];
	count = atoi(argv[2]);
    host = argv[3];
    port = atoi(argv[4]);
	in_buf_size = count * sysconf(_SC_PAGESIZE);
	out_buf_size = count * sysconf(_SC_PAGESIZE);
	
	//printf("job_size = %d K bytes, result_size=%d K bytes.\n", count*4, count*4);

	//printf("Request to open FPGA device:\n");
	gettimeofday(&t1, NULL);
    in_buffer = fpga_acc_open(&(my_context), acc_name, in_buf_size, out_buf_size, host, port);
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &dt);

	if (in_buffer == NULL) {
		printf ("Open %s fail.\n", acc_name);
	}
	else {
        open_sec = dt.tv_usec + 1000000 * dt.tv_sec;
	    //printf("[OPEN] %s takes %ld microseconds.\n", acc_name, open_sec);
		gettimeofday(&t1, NULL);
        sent_block_size = 0;
        left_block_size = in_buf_size;
        chunk_size = my_context.in_buf_size;
        //printf("chunk_size = %lu\n", chunk_size);
        memset(in_buffer, 0, chunk_size);
        while(sent_block_size < in_buf_size){
            ret = fpga_acc_do_job(&my_context, param, chunk_size, &result_buf);
            //printf("ret=%ld\n", ret);
            sent_block_size += ret;
            left_block_size -= ret;
            chunk_size = left_block_size > chunk_size ? chunk_size: left_block_size;
		    if(ret < 0) {
		        printf ("do job fail. code = 0x%lu\n", ret);
                break;
            }
        }

	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &dt);
    exe_sec = dt.tv_usec + 1000000 * dt.tv_sec;
	//printf("[EXECUTION] %s takes %ld microseconds.\n", acc_name, exe_sec);

    gettimeofday(&t1, NULL);
    fpga_acc_close(&my_context);
    gettimeofday(&t2, NULL);
    timersub(&t2, &t1, &dt);
    close_sec = dt.tv_usec + 1000000 * dt.tv_sec;
	//printf("[CLOSE] %s takes %ld microseconds.\n", acc_name, close_sec);
    printf("[%s %d Kbytes]: [OPEN]: %ld [EXE]: %ld [CLOSE]: %ld\n", acc_name, count, open_sec, exe_sec, close_sec);

    }
	return 0;
}

