#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <net/if.h>
#include <fcntl.h>
#include <libgen.h>
#include "acc_servicelayer.h"
#include "rdma_client.h"

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

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

struct context_t {
  struct ibv_context *ctx;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_comp_channel *comp_channel;
};

struct connection_t {
  struct rdma_cm_id *id;
  struct ibv_qp *qp;

  char *recv_region;
  char *send_region;
  struct message *send_msg;
  struct message *recv_msg;

  struct ibv_mr *recv_region_mr;
  struct ibv_mr *send_region_mr;

  struct ibv_mr *recv_msg_mr;
  struct ibv_mr *send_msg_mr;
  uint64_t peer_addr;
  uint32_t peer_rkey;
};

struct rdma_client_context_t {
    char ipaddr[16];
    char port[16];
    char section_id[16];
    char status[16];
    char acc_name[16];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    void * in_buf;
    void * out_buf;
    void * acc_handler;
    struct rdma_cm_id *id;
    struct rdma_event_channel *ec;
};

struct context_t *s_ctx = NULL;
const int TIMEOUT_IN_MS = 500; /* ms */

//int build_connection_to_rdma_server(void *acc_ctx);
//unsigned int remote_rdma_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf);
//void disconnect_with_rdma_server(void *acc_ctx);

void die(const char *reason);
unsigned long client_rdma_data_transfer (void *acc_context, void **result_buf);
int on_setup_event(struct rdma_cm_event *event, void *acc_ctx);
void client_rdma_disconnect (void *acc_context);

void client_rdma_setup_connection(void *acc_context);
int on_addr_resolved(struct rdma_cm_id *id, void *acc_ctx);
void free_rdma_memory(void *acc_context);

void build_connection(struct rdma_cm_id *id, void *acc_ctx);
void build_context(struct ibv_context *verbs, void *acc_ctx);
//void build_rdma_client_context(void *acc_ctx);
void register_memory(struct connection_t *conn, void *acc_ctx);
void post_receive_for_msg(struct connection_t *conn);
void build_params(struct rdma_conn_param *params);
void send_mr(void *context);
int on_connection(struct rdma_cm_id *id);
int on_route_resolved(struct rdma_cm_id *id);
void send_message(struct connection_t *conn);
void poll_cq(void *ctx, void *acc_ctx);
void on_completion(struct ibv_wc *wc);
void build_qp_attr(struct ibv_qp_init_attr *qp_attr, void *acc_context);
void client_write(struct connection_t *conn, uint32_t len, void *acc_ctx);
void post_receive(struct connection_t *con);
int on_disconnect(struct rdma_cm_id *id);
void write_remote(struct connection_t * conn, uint32_t len);

void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}

void build_rdma_client_context(void * acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    acc_context->rdma_context = malloc(sizeof(struct rdma_client_context_t));

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

    struct scheduler_context_t * scheduler_ctx = (struct scheduler_context_t *)(acc_context->scheduler_context);


    strcpy(rdma_context->ipaddr, scheduler_ctx->server_host);

    sprintf(rdma_context->port, "%d", scheduler_ctx->server_port);

    strcpy(rdma_context->acc_name, acc_context->acc_name);

    strcpy(rdma_context->section_id, scheduler_ctx->section_id);

    rdma_context->in_buf_size = acc_context->in_buf_size;

    rdma_context->out_buf_size = acc_context->out_buf_size;



}
int build_connection_to_rdma_server(void * acc_ctx){

    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

    struct addrinfo *addr;
    struct rdma_cm_event *event = NULL;
    struct rdma_cm_id *conn_id = NULL;
    struct rdma_event_channel *ec = NULL;

    printf("RDMA server host = %s, port = %s\n", rdma_context->ipaddr, rdma_context->port);

    TEST_NZ(getaddrinfo(rdma_context->ipaddr, rdma_context->port, NULL, &addr));

    TEST_Z(ec = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(ec, &conn_id, NULL, RDMA_PS_TCP));
    TEST_NZ(rdma_resolve_addr(conn_id, NULL, addr->ai_addr, TIMEOUT_IN_MS));

    freeaddrinfo(addr);
    while (rdma_get_cm_event(ec, &event) == 0) {
        struct rdma_cm_event event_copy;

        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        if (on_setup_event(&event_copy, acc_ctx))
        {
            printf("set up connection successfully\n");
            rdma_context->ec = ec;
            rdma_context->id = conn_id;
            break;
        }
    }

    printf("wait for mr_send_completion\n");
    poll_cq(NULL, acc_ctx);//wait for send_completion
    printf("wait for mr_recv_completion\n");
    poll_cq(NULL, acc_ctx);//wait for recv_completion
    printf("set up connection successfully\n");
    return 0;
}

unsigned int remote_rdma_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

    struct connection_t *conn = (struct connection_t *)rdma_context->id->context;

    client_write(conn, rdma_context->in_buf_size, acc_ctx);
    *result_buf = rdma_context->out_buf;

    return rdma_context->in_buf_size;
}
void disconnect_with_rdma_server(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

    struct connection_t *conn = (struct connection_t *)rdma_context->id->context;
    struct rdma_cm_event *event = NULL;

    uint32_t len = 0|(1UL << 31);
    client_write(conn, len, acc_ctx);

    printf("ready to disconnect\n");
    rdma_disconnect(rdma_context->id);
    printf("send disconnect\n");

    while(1){
        if(rdma_get_cm_event(rdma_context->ec,&event) == 0)
        {
            struct rdma_cm_event event_copy;
            memcpy(&event_copy, event, sizeof(*event));
            rdma_ack_cm_event(event);

            if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED){
                int i = on_disconnect(event_copy.id);
                rdma_destroy_event_channel(rdma_context->ec);
                break;
            }
        }
    }
    return;
}

void free_rdma_memory(void *acc_context){
    return;
}

int on_setup_event(struct rdma_cm_event *event, void *acc_ctx)
{

    int r = 0;

    if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED)
      r = on_addr_resolved(event->id, acc_ctx);
    else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED)
      r = on_route_resolved(event->id);
    else if (event->event == RDMA_CM_EVENT_ESTABLISHED)
      r = on_connection(event->id);
    else if (event->event == RDMA_CM_EVENT_DISCONNECTED){
      r = on_disconnect(event->id);
      printf("disconnect abnormally, fail to open acc\n");
    }
    else
      die("on_event: unknown event.");

    return r;
}
int on_addr_resolved(struct rdma_cm_id *id, void *acc_ctx){
    printf("address resolved.\n");
    printf("build context and connection struct\n");
    build_connection(id, acc_ctx);

    TEST_NZ(rdma_resolve_route(id, TIMEOUT_IN_MS));

    return 0;
}
void build_connection(struct rdma_cm_id *id, void *acc_ctx){

    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);


    struct ibv_qp_init_attr qp_attr;
    struct connection_t *conn;
    build_context(id->verbs, acc_ctx);
    build_qp_attr(&qp_attr, acc_ctx);

    TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

    id->context = conn = (struct connection_t *)malloc(sizeof(struct connection_t));

    conn->id = id;
    conn->qp = id->qp;


    register_memory(conn, acc_ctx);
    post_receive_for_msg(conn);

    return;
}
void build_context(struct ibv_context *verbs, void * acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

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

}
void register_memory(struct connection_t *conn, void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);


    unsigned int send_buf_size = rdma_context->in_buf_size;
    unsigned int recv_buf_size = rdma_context->out_buf_size;
    conn->send_msg = malloc(sizeof(struct message));
    conn->recv_msg = malloc(sizeof(struct message));

    conn->send_region = malloc(send_buf_size);
    conn->recv_region = malloc(recv_buf_size);


    memset(conn->send_region, 0, send_buf_size);
    memset(conn->recv_region, 0, recv_buf_size);

    acc_context->in_buf = conn->send_region;
    //acc_context->out_buf = conn->recv_region;

    TEST_Z(conn->send_region_mr = ibv_reg_mr(
        s_ctx->pd, 
        conn->send_region, 
        send_buf_size, 
        IBV_ACCESS_LOCAL_WRITE));

    TEST_Z(conn->recv_region_mr = ibv_reg_mr(
        s_ctx->pd, 
        conn->recv_region, 
        recv_buf_size, 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    TEST_Z(conn->send_msg_mr = ibv_reg_mr(
        s_ctx->pd, 
        conn->send_msg, 
        sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE));

    TEST_Z(conn->recv_msg_mr = ibv_reg_mr(
        s_ctx->pd, 
        conn->recv_msg, 
        sizeof(struct message), 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    return;
}
void post_receive_for_msg(struct connection_t *conn){
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)conn->recv_msg;
    sge.length = sizeof(*conn->recv_msg);
    sge.lkey = conn->recv_msg_mr->lkey;

    TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));
}
int on_route_resolved(struct rdma_cm_id *id)
{
    struct rdma_conn_param cm_params;

    printf("route resolved.\n");
    build_params(&cm_params);
    TEST_NZ(rdma_connect(id, &cm_params));

    return 0;
}
void build_params(struct rdma_conn_param *params)
{
    memset(params, 0, sizeof(*params));
    params->initiator_depth = params->responder_resources = 1;
    params->rnr_retry_count = 7; /* infinite retry */
}
int on_connection(struct rdma_cm_id *id)
{
  send_mr(id->context);

  return 1;//exit while loop
}

void send_mr(void *context)
{
    struct connection_t *conn = (struct connection_t *)context;
    conn->send_msg->data.mr.addr = (uintptr_t)conn->recv_region_mr->addr;
    conn->send_msg->data.mr.rkey = conn->recv_region_mr->rkey;

    send_message(conn);
}


void send_message(struct connection_t *conn){

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    //printf("posting send message...\n");

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

int on_disconnect(struct rdma_cm_id *id)
{
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

    rdma_destroy_id(id);

    free(conn);


    return 1;
}

void poll_cq(void *ctx, void *acc_ctx)
{
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);


  struct ibv_cq *cq;
  struct ibv_wc wc;
  int ne;

    TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    do {
        ne = ibv_poll_cq(cq, 1, &wc);
        if(ne < 0){
            printf("fail to poll completion from the CQ. ret = %d\n", ne);
            return;
        }
        else if(ne == 0)
            continue;
        else
            on_completion(&wc);
    } while (ne == 0);

  return;
}

void on_completion(struct ibv_wc *wc)
{
    struct connection_t *conn = (struct connection_t *)(uintptr_t)wc->wr_id;

    if (wc->status !=IBV_WC_SUCCESS)
        die("on_completion: status is not ibv_wc_success.");

    if(wc->opcode == IBV_WC_SEND){
        printf("send completed successfully\n");
  }

    else if(wc->opcode == IBV_WC_RECV){
        printf("received MR from server\n");
        conn->peer_addr = conn->recv_msg->data.mr.addr;
        conn->peer_rkey = conn->recv_msg->data.mr.rkey;
    }
    else if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM)
    {
        printf("received result from server\n");
        uint32_t len = ntohl(wc->imm_data);
        uint32_t size = len&(~(1U<<31));
        printf("recv_size =%u\n",size);

    }
}

void client_write(struct connection_t *conn, uint32_t len, void *acc_ctx){
    post_receive(conn);
    write_remote(conn,len);

    printf("wait for write completion\n");
    poll_cq(NULL, acc_ctx);//wait for write completion
    printf("wait for recv_remote_buffer compeltion\n");
    poll_cq(NULL, acc_ctx);//wait for recv completion

}

void post_receive(struct connection_t *conn)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = (uintptr_t)conn;
    wr.next = NULL;
    wr.num_sge = 0;

    TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));
}

void write_remote(struct connection_t * conn, uint32_t len){

    uint32_t size =len&(~(1U<<31));
    //snprintf(conn->send_region, len, "message from active/client side with pid %d", getpid());
    struct ibv_send_wr wr, *bad_wr = NULL; 
    struct ibv_sge sge;

    memset(&wr,0,sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;

    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_addr;
    wr.wr.rdma.rkey = conn->peer_rkey;

    if (size>0){
        wr.sg_list = &sge;
        wr.num_sge = 1;
    sge.addr = (uintptr_t)conn->send_region;
    sge.length = size;
    sge.lkey = conn->send_region_mr->lkey;
    }
    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
    return;
}

void build_qp_attr(struct ibv_qp_init_attr *qp_attr, void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;

    struct rdma_client_context_t * rdma_context = (struct rdma_client_context_t *)(acc_context->rdma_context);

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
