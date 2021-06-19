/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-19 20:05:38
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /rdma_demo/hello_rdma.cc
 */

#include <infiniband/verbs.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

struct rdma_context_t {
public:
    struct ibv_context* ctx;
    struct ibv_pd* pd;
    struct ibv_mr* mr;
    struct ibv_cq* cq;
    struct ibv_qp** qp;
    struct ibv_srq* srq;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;
    int num_qps;
    char* ib_buf;
    size_t ib_buf_size;
};

rdma_context_t g_context;

static void open_device(rdma_context_t* context)
{
    int _num_dev = 0;
    struct ibv_device* _dev = NULL;
    struct ibv_device** _dev_list = NULL;

    // 1.1 获得RDMA列表
    _dev_list = ibv_get_device_list(&_num_dev);
    if (_num_dev == 0) {
        printf("Failed to get RDMA device list.\n");
        exit(1);
    } else {
        printf("Succeed to RDMA device list.[%d]\n", _num_dev);
        _dev = *_dev_list; // used first
    }

    // 1.2 打开RDMA设备
    context->ctx = ibv_open_device(_dev);
    if (context->ctx == NULL) {
        printf("Failed to open RDMA device.\n");
        exit(2);
    }

    // 1.3 返回RDMA设备的端口的属性。
    ibv_query_port(context->ctx, 1, &context->port_attr);

    // 1.4 查询设备获得设备属性
    ibv_query_device(context->ctx, &context->dev_attr);
}

static void create_qpair(rdma_context_t* context)
{
    // 创建一个保护域，protection domain。
    // protection domain可以看作是一个内存保护单位，在内存区域和队列直接建立一个关联关系，防止未授权的访问。
    context->pd = ibv_alloc_pd(context->ctx);

    // 为RDMA设备上下文创建完成队列
    context->cq = ibv_create_cq(context->ctx, context->dev_attr.max_cqe, NULL, NULL, 0);
}

static void register_memory_region(rdma_context_t* context)
{
    // 注册一段内存区域的函数
    context->ib_buf_size = 2UL * 1024 * 1024;
    context->ib_buf = (char*)memalign(4096, context->ib_buf_size); // 申请一段内存
    context->mr = ibv_reg_mr(context->pd, (void*)context->ib_buf,
        context->ib_buf_size,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
}

static void do_recv()
{
    /* create srq */
    /*
    struct ibv_srq_init_attr srq_init_attr;
    srq_init_attr.attr.max_wr = _ib.dev_attr.max_srq_wr;
    srq_init_attr.attr.max_sge = 1;
    _ib.srq = ibv_create_srq(_ib.pd, &srq_init_attr);

    struct ibv_qp_init_attr qp_init_attr;
    qp_init_attr.send_cq = _ib.cq; 
    qp_init_attr.recv_cq = _ib.cq;
    qp_init_attr.srq = _ib.srq; 
    qp_init_attr.cap.max_send_wr = _ib.dev_attr.max_qp_wr;
    qp_init_attr.cap.max_recv_wr = _ib.dev_attr.max_qp_wr;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_type = IBV_QPT_RC;

    _ib.qp = (struct ibv_qp**)calloc(_ib.num_qps, sizeof(struct ibv_qp*));
    for (int i = 0; i < _ib.num_qps; i++) {
        _ib.qp[i] = ibv_create_qp(_ib.pd, &qp_init_attr);
    }
    */
}

static void do_send()
{
}

int main(int argc, char** argv)
{
    rdma_context_t _ctx;
    memset(&_ctx, 0, sizeof(_ctx));
    open_device(&_ctx);
    return 0;
}