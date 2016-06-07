#include "acc_servicelayer.h"
#include "tcp_transfer.h"


size_t send_msg(int fd, void *buffer, size_t len, size_t chunk)
{
    char *c_ptr = (char *) buffer;
    size_t block = chunk, left_block = len, sent_block = 0;

    if (len <= chunk){ 
        printf("send small message ........\n");
        return send(fd, buffer, len, 0);
    }

    while(sent_block < len) {
        block = send(fd, c_ptr, chunk, 0);
        if (block < 0){
            printf("Fail to send");
            printf("sent block =%d\n", block);
        }
        if (block == 0){
            printf("nothing to send\n");
            break;
        }
        sent_block += block;
        left_block -= block;
        chunk = left_block > chunk ? chunk : left_block;
        c_ptr += block;
        //printf("sent block =%d\n", sent_block);

    }
    return sent_block; 
}
    
size_t recv_msg(int fd, void *buffer, size_t len, size_t chunk)
{
    char *c_ptr = buffer;
    size_t block = chunk, left_block = len, recv_block = 0;
    int stop_flag = 0;

    if (len <= chunk){ 
        //printf("recv small message ........\n");
        return recv(fd, buffer, len, 0);
    }

    while (recv_block < len) {
        //printf("recv.....\n");
        block = recv(fd, c_ptr, chunk, 0);
        if (block == 0){
            //printf("nothing to recv\n");
            break;
        }

        recv_block += block;
        left_block -= block;
        chunk = left_block > chunk ? chunk : left_block;
        c_ptr += block;
        //printf("recv block %zu\n", recv_block);
    }
    return recv_block; 
}


void build_socket_context(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    acc_context->tcp_context = malloc(sizeof(struct tcp_client_context_t));
    struct tcp_client_context_t * tcp_ctx = (struct tcp_client_context_t *)(acc_context->tcp_context);

    memset((char *)(acc_context->tcp_context), 0, sizeof(struct tcp_client_context_t));

    struct scheduler_context_t * scheduler_ctx = (struct scheduler_context_t *)(acc_context->scheduler_context);

    strcpy(tcp_ctx->server_host, scheduler_ctx->server_host);
    tcp_ctx->server_port = scheduler_ctx-> server_port;
    //printf("host=%s, port=%d\n", tcp_ctx->server_host, tcp_ctx->server_port);
    return;

}

int build_connection_to_tcp_server(void *acc_ctx) {
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct tcp_client_context_t *tcp_ctx = (struct tcp_client_context_t *)acc_context->tcp_context;
    struct hostent *hp;	/* host information */
	int sockoptval = 1;
    int client_fd;
    unsigned int in_buf_size = acc_context->in_buf_size;
    unsigned int out_buf_size = acc_context->out_buf_size;
    int port = tcp_ctx->server_port;
    char *host = tcp_ctx->server_host;
    //printf("host=%s\n",host);
    struct sockaddr_in *my_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *server_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_NEG(client_fd);
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    memset((char *)my_addr, 0, sizeof(struct sockaddr));
    my_addr->sin_family = AF_INET;
    my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr->sin_port = htons(0);

    if(bind(client_fd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) < 0){
        perror("bind failed");
        close(client_fd);
        return -1;
    }

    memset((char *)server_addr, 0, sizeof(struct sockaddr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);
    hp = gethostbyname(host);
    if (!hp) {
        close(client_fd);
        return -1;
    }
    memcpy((void *)&(server_addr->sin_addr), hp->h_addr_list[0], hp->h_length);


    if (connect(client_fd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) <0){
        perror("connect failed");
        close(client_fd);
        return -1;
    }
    tcp_ctx->to_server_addr = (void *)my_addr;
    tcp_ctx->server_addr = (void *)server_addr;
    tcp_ctx->to_server_fd = client_fd;
    tcp_ctx->in_buf = malloc(in_buf_size);
    tcp_ctx->out_buf = malloc(out_buf_size);
    acc_context->in_buf = tcp_ctx->in_buf;
    return client_fd;
}



unsigned int remote_tcp_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf) {
    //printf("job_len = %d\n", job_len);
	struct timeval t1, t2, dt;
    unsigned long send_sec, recv_sec;
    struct acc_context_t *acc_context = (struct acc_context_t *) acc_ctx;
    struct tcp_client_context_t *tcp_ctx = (struct tcp_client_context_t *)acc_context->tcp_context;
    unsigned int recv_buf_size;
    *result_buf = tcp_ctx->out_buf;
    char *in_buf = (char *)tcp_ctx->in_buf;
    int fd = tcp_ctx->to_server_fd;
    gettimeofday(&t1, NULL);
    size_t len = send_msg(fd, in_buf, job_len, MTU); 
    recv_buf_size = recv_msg(fd, *result_buf, job_len, MTU);
    gettimeofday(&t2, NULL);
    timersub(&t2,&t1,&dt);
    recv_sec = (dt.tv_usec + 1000000* dt.tv_sec)/1000;
    //printf("Round Time %ld\n",recv_sec );

    return recv_buf_size; 
} 

void free_tcp_memory(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct tcp_client_context_t *tcp_ctx = (struct tcp_client_context_t *)(acc_context->tcp_context);
    free(tcp_ctx->server_addr);
    free(tcp_ctx->to_server_addr);
    free(tcp_ctx->out_buf);
    free(tcp_ctx->in_buf);
    free(tcp_ctx);

}

int socket_server_open(void *server_param){
    struct server_param_t * my_param = (struct server_param_t *)server_param;
    pthread_t server_thread;
    int thread_status;
    if (pipe(my_param->pipe) < 0) {
        //printf("Open pipe error.\n");
        strcpy(my_param->status, "1");
        return -1;
    }
    if(pthread_create(&server_thread, NULL, tcp_server_data_transfer, my_param)!=0) {
        fprintf(stderr, "Error creating thread\n");
        strcpy(my_param->status,"1");
        return -2;
    }
    if (read(my_param->pipe[0], &thread_status, sizeof(int))== -1){
        printf("Fail to read status from thread\n");
        strcpy(my_param->status,"1");
        return -3;
    }

    sprintf(my_param->status, "%d", thread_status);

    if (thread_status == 0){
        pthread_detach(server_thread);
        printf("thread created successfully.\n");
        if (DEBUG){
            //printf("server listening to %s, port %d\n", my_param->ipaddr, (atoi)(my_param->port));
        }
        return 0;
    }
    else{
        printf("Fail to open local ACC.\n");
        return 4;
    }

}

void * tcp_server_data_transfer(void * server_param) {

    struct server_param_t * my_param = (struct server_param_t *) server_param;
    struct tcp_server_context_t server_context;
    struct acc_handler_t acc_handler;
    memset((void *)&server_context,0,sizeof(server_context));
    server_context.acc_handler = &(acc_handler);
    strcpy(server_context.job_id, my_param->job_id);
    strcpy(server_context.section_id, my_param->section_id);
    strcpy(server_context.acc_name, my_param->acc_name);
    strcpy(server_context.scheduler_host, my_param->scheduler_host);
    strcpy(server_context.scheduler_port, my_param->scheduler_port);
    server_context.in_buf_size = my_param->in_buf_size;
    server_context.out_buf_size = my_param->out_buf_size;
    unsigned int real_buf_size = my_param->real_in_buf_size;
    //printf("my_param->in_buf_size=%u, out_buf_size=%u\n",my_param->in_buf_size, my_param->out_buf_size);


    int status = tcp_local_fpga_open((void *)&server_context);
    sprintf(my_param->status, "%d", status);
    if (status != 0) {
        printf("Fail to open %s.\n", my_param->acc_name);
        write(my_param->pipe[1], &status, sizeof(int));
        return NULL;
    }

    /*open a server socket */
    struct sockaddr_in server_addr, client_addr;
    memset((void *)&server_addr, 0, sizeof(server_addr));
    memset((void *)&client_addr, 0, sizeof(client_addr));
    int server_fd;//listening socket providing service
    int rqst_fd;//socket accepting the request;
    int sockoptval = 1;
    char server_host[16];
    unsigned short server_port;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(0);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("bind failed\n");
        sprintf(my_param->status, "%d", 2);
        exit(1);
    }
    /*set up the socket for listening with a queue length of BACKLOG */

    if(listen(server_fd, BACKLOG) < 0) {
        printf("listen failed.\n");
        sprintf(my_param->status, "%d", 2);
        exit(1);
    }

	socklen_t alen = sizeof(server_addr);     /* length of address */
    getsockname(server_fd, (struct sockaddr *)&server_addr, &alen);
    server_port = ntohs(server_addr.sin_port);
    strcpy(server_host, inet_ntoa(server_addr.sin_addr));
    strcpy(my_param->ipaddr, server_host);
    sprintf(my_param->port, "%d", server_port);
    write(my_param->pipe[1], &status, sizeof(int));


    while ((rqst_fd = accept(server_fd, (struct sockaddr *)&client_addr, &alen)) <0 ) {
        printf("accept failed\n");
        //exit (1);
    }
    //printf("received connection from: %s, port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    //size_t recv_len;
    size_t recv_len = (size_t) server_context.in_buf_size;
    size_t send_len = (size_t) server_context.out_buf_size;
    size_t recv_size, send_size;
    //printf("recv_len = %zu, send_len=%zu\n", recv_len, send_len);
    //printf("server_context->in_buf_size=%u, out_buf_size=%u\n",server_context.in_buf_size, server_context.out_buf_size);

    char *send_buff = (char *)memset(malloc(send_len),0, send_len); //buffer
    char *recv_buff = (char *)memset(malloc(recv_len),0, recv_len); //buffer

    while (1) {
        if( (recv_size = recv_msg(rqst_fd, recv_buff, recv_len, MTU)) < 0){
            printf("fail to receive\n");
            exit(1);
        }
        if (recv_size == 0){
            printf("recv size = 0, disconnect\n");
            tcp_local_fpga_close((void *)&server_context);
            server_report_to_scheduler((void *)&server_context);
            break;
        }
        /*do jo*/ 
        printf("recv size =%ld\n", recv_size);
        memcpy(server_context.in_buf, recv_buff, recv_size);
        unsigned long ret = tcp_local_fpga_do_job((void *)&server_context, recv_size);
        //printf("ret = %lu\n", ret);
        memcpy(send_buff, server_context.out_buf, ret);
        
        if ( (send_size = send_msg(rqst_fd, send_buff, ret, MTU))< 0){
            printf("fail to send\n");
            exit(1);
        }
        printf("send size from server:%zu\n", send_size);
        real_buf_size -= recv_len;
        recv_len = real_buf_size > recv_len ? recv_len : real_buf_size;
        if (recv_len == 0)
            recv_len += MTU;
    
    }

    close(rqst_fd);
    close(server_fd);
    return NULL;
}


void disconnect_with_tcp_server(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *) acc_ctx;
    struct tcp_client_context_t *tcp_ctx = (struct tcp_client_context_t *)acc_context->tcp_context;
    int fd = tcp_ctx->to_server_fd;
    close(fd);
    return;
}

int tcp_local_fpga_open(void * server_context){
    struct tcp_server_context_t * my_context = (struct tcp_server_context_t *)server_context;
    int section_id = atoi(my_context->section_id);
    my_context->in_buf = pri_acc_open((struct acc_handler_t *)(my_context->acc_handler), my_context->acc_name, my_context->in_buf_size, my_context->out_buf_size, section_id);
    if(my_context->in_buf == NULL){
        printf("open %s fails.\n", my_context->acc_name);
        return -1;
    }
    printf("open %s successfully\n", my_context->acc_name);

    return 0;//return status;
}

unsigned long tcp_local_fpga_do_job(void * server_context, unsigned int len){
    struct tcp_server_context_t * my_context = (struct tcp_server_context_t *)server_context;
    char param[16];
    param[0]=0x24;
    void *result_buf = NULL;
    long ret = acc_do_job((struct acc_handler_t *)(my_context->acc_handler), param, len, &(result_buf));
    my_context->out_buf = result_buf;
    printf("do job finished\n");
    return ret;
}

void tcp_local_fpga_close(void * server_context){
    struct tcp_server_context_t * my_context = (struct tcp_server_context_t *)server_context;
    acc_close((struct acc_handler_t *)(my_context->acc_handler));
    printf("tcp local fpga close\n");
    return;
}
    
void server_report_to_scheduler(void * server_ctx){
    struct debug_context_t debug_ctx;
    char response[16];
    struct tcp_server_context_t *server_context = server_ctx;
    struct hostent *hp;	/* host information */

    struct sockaddr_in *my_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *scheduler_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    int my_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_NEG(my_fd);

    int sockoptval = 1;
	setsockopt(my_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));

    memset((char *)my_addr, 0, sizeof(struct sockaddr));
    my_addr->sin_family = AF_INET;
    my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr->sin_port = htons(0);
    if(bind(my_fd, my_addr, sizeof(struct sockaddr)) < 0){
        perror("bind failed");
        close(my_fd);
        return;
    }

    memset((char *)scheduler_addr, 0, sizeof(struct sockaddr));
    scheduler_addr->sin_family = AF_INET;
    scheduler_addr->sin_port = htons(atoi(server_context->scheduler_port));
    hp = gethostbyname(server_context->scheduler_host);
    //printf("scheduler host, port:%s,%s\n", server_context->scheduler_host, server_context->scheduler_port);
    memcpy((void *) &(scheduler_addr->sin_addr), hp->h_addr_list[0], hp->h_length);

    if (connect(my_fd, (struct sockaddr *)scheduler_addr, sizeof(struct sockaddr)) <0){
        perror("connect failed");
        close(my_fd);
        return;
    }


    memset(response,0, 16);

    memset((char *)&debug_ctx, 0, sizeof(debug_ctx));
    strcpy(debug_ctx.status,"close");
    strcpy(debug_ctx.job_id, server_context->job_id);
    send(my_fd, (char *)&debug_ctx, sizeof(struct debug_context_t), 0);
    recv(my_fd, response, 16, 0);
    //printf("response from schduler: %s\n", response);
    return;

}
