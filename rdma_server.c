#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <net/if.h>
#include <memory.h>
#include <fcntl.h>
#include <libgen.h>
#include "acc_servicelayer.h"
#include "fpga-sim/driver/fpga-libacc.h" 
#include "rdma_server.h"
#define TIMEOUT_IN_MS 500 
#define RDMA_DEBUG 1

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)
#define TEST_NEG(x) do { if ( (x) < 0 ) exit(1); } while (0)
#define INTERFACE "ib01"

struct message
{
  union
  {
    struct {
        uint64_t addr;
        uint32_t rkey;
    }mr;
  } data;
};

struct context_t{
  struct ibv_context *ctx;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_comp_channel *comp_channel;
  pthread_t cq_poller_thread;
};


struct connection_t {
  struct rdma_cm_id *id;
  struct ibv_qp *qp;

  char *recv_region;
  char *send_region;

  struct message *recv_msg;
  struct message *send_msg;

  struct ibv_mr *recv_region_mr;
  struct ibv_mr *send_region_mr;

  struct ibv_mr *recv_msg_mr;
  struct ibv_mr *send_msg_mr;

  uint64_t peer_addr;
  uint32_t peer_rkey;

};

struct rdma_context_t {
    struct sockaddr_in addr;
    uint16_t port;
    struct rdma_cm_event *event;
    struct rdma_cm_id *listener;
    struct rdma_event_channel *ec;
    struct connection_t *connection;
};


struct rdma_server_context_t {
    char section_id[16];
    char status[16];
    char acc_name[64];
    char scheduler_host[16];
    char scheduler_port[16];
    char job_id[16];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    void * in_buf;
    void * out_buf;
    void * rdma_context;
    void * acc_handler;
};

struct context_t *s_ctx = NULL;

void die(const char *reason);


//void rdma_server_open(void *server_param);
void * rdma_server_data_transfer(void * server_param);
int on_connect_request(struct rdma_cm_id *id, void *rdma_context);
int on_server_setup_event(struct rdma_cm_event *event, void * server_context );

int rdma_local_fpga_open(void *server_ctx);

void register_memory(void *server_ctx);
void build_connection(struct rdma_cm_id *id, void *server_context);
void build_context(struct ibv_context *verbs, void * rdma_ctx);

void build_params(struct rdma_conn_param *params);
void build_qp_attr(struct ibv_qp_init_attr *qp_attr, void *server_context);
void post_receive_for_msg(struct connection_t *con);
void poll_cq(struct rdma_server_context_t *server_context);

int on_completion(struct ibv_wc *wc, struct rdma_server_context_t *server_context);

void post_receive(struct connection_t *con);
void send_mr(struct connection_t *connection );
void local_fpga_do_job(struct rdma_server_context_t * server_context);

void server_write_remote(struct connection_t * conn, uint32_t len);
void send_message(struct connection_t *conn);
int on_disconnect_event(struct rdma_cm_event *event, struct rdma_server_context_t *server_context);
int on_disconnect(struct rdma_cm_id *id, struct rdma_server_context_t *server_context);
void local_fpga_close(void *server_context);
char * get_server_ip_addr(char * interface);
void report_to_scheduler(void *server_param);


void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}


void rdma_server_open(void *server_param){
    struct server_param_t * my_param = (struct server_param_t *)server_param;
    pthread_t server_pthread;
    int thread_status;
    if(pipe(my_param->pipe) <0) {
        printf("Open pipe error.\n");
        strcpy(my_param->status, "1");
        return;
    }

    if(pthread_create(&server_pthread, NULL, rdma_server_data_transfer, my_param)!= 0){
        fprintf(stderr, "Error creating thread\n");
        strcpy(my_param->status,"1");
        return;
    }
    if (read(my_param->pipe[0], &thread_status, sizeof(int))== -1){
        printf("Fail to read status from thread\n");
        strcpy(my_param->status,"1");
        return;
    }
    pthread_detach(server_pthread);
    sprintf(my_param->status, "%d", thread_status);
    if (RDMA_DEBUG){
        printf("Status from server:%s.\n", my_param->status); 
        printf("Server listening to port %d.\n", (atoi)(my_param->port));
        printf("Thread created successfully.\n");
    }
    return;
}

void * rdma_server_data_transfer(void * server_param){
    	struct      server_param_t * my_param = (struct server_param_t *)server_param;
        struct rdma_server_context_t server_context;
        struct acc_handler_t acc_handler;
        memset(&acc_handler, 0, sizeof(acc_handler));
        memset((void *)&server_context, 0, sizeof(server_context));

	    server_context.acc_handler = &(acc_handler);
        strcpy(server_context.section_id, my_param->section_id);
        strcpy(server_context.status, my_param->status);
        strcpy(server_context.acc_name, my_param->acc_name);
        strcpy(server_context.job_id, my_param->job_id);
        strcpy(server_context.scheduler_host, my_param->scheduler_host);
        strcpy(server_context.scheduler_port, my_param->scheduler_port);

        server_context.in_buf_size = my_param->in_buf_size;
        server_context.out_buf_size = my_param->out_buf_size;

        int status = rdma_local_fpga_open((void *)&server_context);
        sprintf(my_param->status, "%d", status);

        if(status != 2){
            printf("Fail to open %s.\n", my_param->acc_name);
            return NULL;
        }

        /*open an RDMA server listening socket*/
        server_context.rdma_context = malloc(sizeof(struct rdma_context_t));
        struct rdma_context_t * rdma_context = (struct rdma_context_t *)(server_context.rdma_context);
        memset((void *)rdma_context,0,sizeof(struct rdma_context_t));

        rdma_context->addr.sin_family = AF_INET;
        TEST_Z(rdma_context->ec = rdma_create_event_channel());
        TEST_NZ(rdma_create_id(rdma_context->ec, &(rdma_context->listener), NULL, RDMA_PS_TCP));
        TEST_NEG(rdma_bind_addr(rdma_context->listener, (struct sockaddr *)&(rdma_context->addr)));
        TEST_NZ(rdma_listen(rdma_context->listener, 10));

        rdma_context->port = ntohs(rdma_get_src_port(rdma_context->listener));
        //char interface = get_server_ip_addr(INTERFACE);
        //strcpy(my_param->ipaddr, interface);

        printf("listening on port %d.\n", rdma_context->port);
        sprintf(my_param->port, "%d", rdma_context->port);
		write(my_param->pipe[1], &(status), sizeof(int));

        while (rdma_get_cm_event(rdma_context->ec, &(rdma_context->event)) == 0) {
            struct rdma_cm_event event_copy;
            memcpy(&event_copy, rdma_context->event, sizeof(struct rdma_cm_event));
            rdma_ack_cm_event(rdma_context->event);

            if(on_server_setup_event(&event_copy, (void *)&server_context))
                break;
        }

        poll_cq(&server_context);

        while (rdma_get_cm_event(rdma_context->ec, &(rdma_context->event)) == 0){
            struct rdma_cm_event event_copy;
            memcpy(&event_copy, rdma_context->event, sizeof(struct rdma_cm_event));
            rdma_ack_cm_event(rdma_context->event);
            if (on_disconnect_event(&event_copy, &server_context))
                break;
        }
        printf("Disconnect ... \n");
        rdma_destroy_event_channel(rdma_context->ec);
        report_to_scheduler(&(server_context));
        return NULL;
}

void report_to_scheduler(void *server_ctx){
    struct debug_context_t debug_ctx;
    struct rdma_server_context_t * server_context = (struct rdma_server_context_t *)server_ctx;
    char response[16];
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

int on_server_setup_event(struct rdma_cm_event *event, void * server_context ){
    int r = 0;
    if(event->event == RDMA_CM_EVENT_CONNECT_REQUEST)
        r = on_connect_request(event->id, server_context);
    else if(event->event == RDMA_CM_EVENT_ESTABLISHED){
        r = 1;
    }
    else
        die("on_event: unknown event.\n");
    return r;
}

int on_connect_request(struct rdma_cm_id *id, void *server_context){
    printf("received connection request.\n");
    struct rdma_conn_param cm_params;
    printf("building connection......\n");
    build_connection(id, server_context);
    printf("building param......\n");
    build_params(&cm_params);
    TEST_NZ(rdma_accept(id, &cm_params));
    return 0;
}


int rdma_local_fpga_open(void *server_ctx){
    struct rdma_server_context_t*  server_context = (struct rdma_server_context_t *) server_ctx;
    int section_id = atoi(server_context->section_id);
    int return_flag = 0;

printf("debug %s %d %d %d\n", server_context->acc_name, server_context->in_buf_size, server_context->out_buf_size, section_id);

    server_context->in_buf = pri_acc_open((struct acc_handler_t * )(server_context->acc_handler), server_context->acc_name, server_context->in_buf_size, server_context->out_buf_size, section_id);

    if (server_context->in_buf == NULL){
        printf("Open %s fail\n", server_context->acc_name);

        return_flag = -1;
    }
    else
        return_flag = 2;
    sprintf(server_context->status, "%d", return_flag);
    return return_flag;

}

void build_connection(struct rdma_cm_id *id, void *server_context){
    struct ibv_qp_init_attr qp_attr;
    struct connection_t *connection;
    struct rdma_context_t * rdma_context = ((struct rdma_server_context_t *)server_context)->rdma_context;
    build_context(id->verbs, rdma_context); 
    build_qp_attr(&qp_attr, server_context); 
	TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));
    
    id->context = connection = (struct connection_t *)malloc(sizeof(struct connection_t));

    rdma_context->connection = connection;
    connection->id = id;
    connection->qp = id->qp;

    register_memory(server_context);
    post_receive_for_msg(connection); 
    return;
}

void build_params(struct rdma_conn_param *params){
    memset(params, 0, sizeof(*params));

    params->initiator_depth = params->responder_resources = 1;
    params->rnr_retry_count = 7; /* infinite retry */
    return;
}

void build_qp_attr(struct ibv_qp_init_attr *qp_attr, void *server_context){
    struct rdma_server_context_t *server_ctx = (struct rdma_server_context_t *)server_context;

    memset(qp_attr, 0, sizeof(*qp_attr));

    qp_attr->send_cq = s_ctx->cq;
    qp_attr->recv_cq = s_ctx->cq;
    qp_attr->qp_type = IBV_QPT_RC;

    qp_attr->cap.max_send_wr = 10;
    qp_attr->cap.max_recv_wr = 10;
    qp_attr->cap.max_send_sge = 1;
    qp_attr->cap.max_recv_sge = 1;
    return;
}
void register_memory(void *server_ctx){
    struct rdma_server_context_t *server_context = (struct rdma_server_context_t *)server_ctx;

    struct rdma_context_t * rdma_context = (struct rdma_context_t *)server_context->rdma_context;
    struct connection_t *connection = rdma_context->connection;

    unsigned int recv_buf_size = server_context->in_buf_size;
    unsigned int send_buf_size = server_context->out_buf_size;

    connection->send_region = malloc(send_buf_size);
    connection->recv_region = malloc(recv_buf_size);

    server_context->in_buf = connection->recv_region;
    connection->send_msg = malloc(sizeof(struct message));
    connection->recv_msg = malloc(sizeof(struct message));
    TEST_Z(connection->send_region_mr = ibv_reg_mr(
      s_ctx->pd, 
      connection->send_region, 
      send_buf_size, 
      IBV_ACCESS_LOCAL_WRITE));

    TEST_Z(connection->recv_region_mr = ibv_reg_mr(
      s_ctx->pd, 
      connection->recv_region, 
      recv_buf_size, 
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    TEST_Z(connection->send_msg_mr = ibv_reg_mr(
      s_ctx->pd, 
      connection->send_msg, 
      sizeof(struct message), 
      IBV_ACCESS_LOCAL_WRITE));

    TEST_Z(connection->recv_msg_mr = ibv_reg_mr(
      s_ctx->pd, 
      connection->recv_msg, 
      sizeof(struct message), 
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    return;
}
void post_receive_for_msg(struct connection_t *connection){
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)connection;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)connection->recv_msg;
    sge.length = sizeof(*connection->recv_msg);
    sge.lkey = connection->recv_msg_mr->lkey;

    TEST_NZ(ibv_post_recv(connection->qp, &wr, &bad_wr));
    return;
}
void build_context(struct ibv_context *verbs, void * rdma_ctx){
    if (s_ctx) {
      if (s_ctx->ctx != verbs)
        die("cannot handle events in more than one context.");

      return;
    }
    s_ctx = (struct context_t *)malloc(sizeof(struct context_t));
    s_ctx->ctx = verbs;

    TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));
    TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
    TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary */
    TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));

    return;
}

void poll_cq(struct rdma_server_context_t *server_context){
    struct ibv_cq *cq;
    struct ibv_wc wc;
    int ret;

    void *ctx = NULL;

    while (1) {
      TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
      ibv_ack_cq_events(cq, 1);
      TEST_NZ(ibv_req_notify_cq(cq, 0));

      while (ibv_poll_cq(cq, 1, &wc)){
        ret = on_completion(&wc, server_context);
      }
      if (ret)
        break;
    }
    return;
}

int on_completion(struct ibv_wc *wc, struct rdma_server_context_t *server_context){
    struct connection_t * connection = (struct connection_t *)(uintptr_t)wc->wr_id;
    if (wc->status != IBV_WC_SUCCESS)
        die("on_completion: status is not IBV_WC_SUCCESS.\n");

    if (wc->opcode == IBV_WC_SEND)
        printf("Send completed successfully.\n");
    else if (wc->opcode == IBV_WC_RECV){
        printf("received MR from client\n");

        connection->peer_addr = connection->recv_msg->data.mr.addr;
        connection->peer_rkey = connection->recv_msg->data.mr.rkey;
        post_receive(connection);
        send_mr(connection);
    }
    else if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM){

        uint32_t len = ntohl(wc->imm_data);
        uint32_t size = len &(~(1U<<31));
        uint32_t flag = len>>31;
        printf("received %u bytes from client\n", size);
	

        if(flag ==1){
            server_write_remote(connection, (uint32_t) 0);
	        return 1;
        }

        else { 
		    //copy input data from RDMA to FPGA
        	memcpy(server_context->in_buf, connection->recv_region, server_context->in_buf_size);

        	//acceleration do job
        	local_fpga_do_job(server_context);

		    //copy output data from FPGA to RDMA
		    memcpy(connection->send_region, server_context->out_buf, server_context->out_buf_size);
            post_receive(connection);
            server_write_remote(connection, (uint32_t) server_context->out_buf_size);
        }  
    }
    return 0;
}


void post_receive(struct connection_t *connection){
    struct ibv_recv_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = (uintptr_t)connection;
    wr.next = NULL;
    wr.num_sge = 0;

    TEST_NZ(ibv_post_recv(connection->qp, &wr, &bad_wr));
    return;
}

void send_mr(struct connection_t *connection ){
    connection->send_msg->data.mr.addr = (uintptr_t)connection->recv_region_mr->addr;
    connection->send_msg->data.mr.rkey = connection->recv_region_mr->rkey;

    send_message(connection);
    return;
}
void send_message(struct connection_t *conn){

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    printf("posting send message...\n");

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)conn->send_msg;
    sge.length = sizeof(struct message);
    sge.lkey = conn->send_msg_mr->lkey;

    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
    return ;

}
void server_write_remote(struct connection_t * conn, uint32_t len){
    uint32_t size =len&(~(1U<<31));
    //sprintf(conn->send_region, "message from passive/server side with pid %d", getpid());

    struct ibv_send_wr wr, *bad_wr = NULL; 
    struct ibv_sge sge;
 
    memset(&wr,0,sizeof(wr));
 
    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
 
    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_addr;
    wr.wr.rdma.rkey = conn->peer_rkey;
 
    if (size >0) {
        wr.sg_list = &sge;
        wr.num_sge = 1;
        sge.addr = (uintptr_t)conn->send_region;
        sge.length = size;
        sge.lkey = conn->send_region_mr->lkey;
    }
    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
    return;
}

int on_disconnect_event(struct rdma_cm_event *event, struct rdma_server_context_t *server_context)
{
  int r = 0;
  if (event->event == RDMA_CM_EVENT_DISCONNECTED){
    r = on_disconnect(event->id, server_context);
  }
  else
    die("on_event: unknown event.");

  return r;
}
int on_disconnect(struct rdma_cm_id *id, struct rdma_server_context_t *server_context){
    struct connection_t *conn = (struct connection_t *)id->context;

    printf("disconnected.\n");

    rdma_destroy_qp(id);

    ibv_dereg_mr(conn->send_region_mr);
    ibv_dereg_mr(conn->recv_region_mr);

    ibv_dereg_mr(conn->send_msg_mr);
    ibv_dereg_mr(conn->recv_msg_mr);

    free(conn->send_region);
    free(conn->recv_region);

    free(conn->recv_msg);
    free(conn->send_msg);

    free(conn);

    rdma_destroy_id(id);
    //close local fpga device
    local_fpga_close(server_context);

  return 1; /* exit event loop */
}
void local_fpga_close(void *server_ctx){
    struct rdma_server_context_t *server_context = (struct rdma_server_context_t *)server_ctx;
    acc_close(server_context->acc_handler);
    printf("Close acc successfully.\n");
    return;
}
void local_fpga_do_job(struct rdma_server_context_t * server_context){
    struct rdma_server_context_t* server_ctx = (struct rdma_server_context_t *)server_context;
    char param[16];
    param[0]=0x24;
    void *result_buf = NULL;
    unsigned int len = server_context->in_buf_size;
    long ret = acc_do_job((struct acc_handler_t *)(server_ctx->acc_handler),param, len, &(result_buf));
    server_context->out_buf = result_buf;
    //server_context->out_buf = malloc(server_context->in_buf_size);
    return;
};

char * get_server_ip_addr(char * interface){
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}
