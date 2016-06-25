void build_rdma_client_context(void *acc_ctx);
int build_connection_to_rdma_server(void *acc_ctx);
unsigned int remote_rdma_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf);
void disconnect_with_rdma_server(void *acc_ctx);
void free_rdma_memory(void *acc_context);
