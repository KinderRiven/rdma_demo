/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-08-05 14:29:03
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /rdma_demo/hello_rdma.cc
 */

#include <arpa/inet.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <malloc.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MSG_SIZE (64)

// static int g_gids[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x0a, 0x00, 0x00, 0x30 };
// 10.0.0.42
static int g_gids[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x0a, 0x00, 0x00, 0x28 };

struct qp_info_t {
    uint64_t addr; // buffer address
    uint32_t rkey; // remote key
    uint32_t qp_num; // QP number
    uint16_t lid; // LID of the IB port
    union ibv_gid gid;
} __attribute__((packed));

struct rdma_context_t {
public:
    char* ib_buf;
    struct ibv_context* ctx;
    struct ibv_pd* pd;
    struct ibv_mr* mr;
    struct ibv_cq* cq;
    struct ibv_qp* qp[128];
    struct ibv_srq* srq;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;
    union ibv_gid gid;
    qp_info_t* local_qp;
    qp_info_t* remote_qp;

public: // need initlizate
    int num_qps;
    size_t ib_buf_size;
};

static void open_device(rdma_context_t* context)
{
    printf("|open_device.\n");
    int _res;
    int _num_dev = 0;
    struct ibv_device* _dev = NULL;
    struct ibv_device** _dev_list = NULL;

    // 获得RDMA列表
    _dev_list = ibv_get_device_list(&_num_dev);
    if (_num_dev == 0) {
        printf("|--ibv_get_device_list failed.\n");
        exit(1);
    }
    printf("|--ibv_get_device_list ok.[%d]\n", _num_dev);
    _dev = _dev_list[0]; // used first

    // 打开RDMA设备
    context->ctx = ibv_open_device(_dev);
    if (context->ctx == NULL) {
        printf("|--ibv_open_device failed.\n");
        exit(1);
    }
    printf("|--ibv_open_device ok.\n");
    _res = ibv_query_port(context->ctx, 1, &context->port_attr);
    if (_res) {
        printf("|--ibv_query_port failed.\n");
        exit(1);
    }
    printf("|--ibv_query_port ok.\n");
    printf("|----[lid:%d]\n", context->port_attr.lid);

    _res = ibv_query_gid(context->ctx, 1, 0, &context->gid);
    if (_res) {
        printf("|--ibv_query_gid failed.\n");
        exit(1);
    }
    for (int i = 0; i < 16; i++) {
        printf("%02x", context->gid.raw[i]);
        if (i & 1) {
            printf(":");
        }
    }
    printf("\n");
    for (int i = 0; i < 16; i++) {
        context->gid.raw[i] = g_gids[i];
        printf("%02x", context->gid.raw[i]);
        if (i & 1) {
            printf(":");
        }
    }
    printf("\n");

    _res = ibv_query_device(context->ctx, &context->dev_attr);
    if (_res) {
        printf("|--ibv_query_device failed.\n");
        exit(1);
    }
    printf("|--ibv_query_device ok.\n");
    printf("|----[VERSION:%d]\n", context->dev_attr.hw_ver);
    printf("|----[MAX_NUM_CQ:%d][MAX_CQE:%d]\n", context->dev_attr.max_cq, context->dev_attr.max_cqe);
    printf("|----[MAX_QP_WR:%d]\n", context->dev_attr.max_qp_wr);
    printf("|----[MAX_NUM_MR:%dMB][MR_SIZE:%lluGB]\n", context->dev_attr.max_mr / (1024 * 1024), context->dev_attr.max_mr_size / (1024UL * 1024 * 1024));
}

static void create_qpair(rdma_context_t* context)
{
    printf("|create_qpair.\n");
    // create protection domain (pd)
    // protection domain可以看作是一个内存保护单位，在内存区域和队列直接建立一个关联关系，防止未授权的访问。
    context->pd = ibv_alloc_pd(context->ctx);
    if (context->pd == NULL) {
        printf("|--ibv_alloc_pd failed.\n");
        exit(1);
    } else {
        printf("|--ibv_alloc_pd ok.\n");
    }

    // create completion queue (cq)
    context->cq = ibv_create_cq(context->ctx, context->dev_attr.max_cqe, NULL, NULL, 0);
    if (context->cq == NULL) {
        printf("|--ibv_create_cq failed.\n");
        exit(1);
    } else {
        printf("|--ibv_create_cq ok.\n");
    }

    // create shared received queue (srq)
    struct ibv_srq_init_attr srq_init_attr;
    srq_init_attr.attr.max_wr = context->dev_attr.max_srq_wr;
    srq_init_attr.attr.max_sge = 1;
    context->srq = ibv_create_srq(context->pd, &srq_init_attr);
    if (context->srq == NULL) {
        printf("|--ibv_create_srq failed.\n");
    } else {
        printf("|--ibv_create_srq ok.\n");
    }

    struct ibv_qp_init_attr qp_init_attr;
    /*
        struct ibv_qp_init_attr {
            void		        *qp_context;
            struct ibv_cq	    *send_cq;
            struct ibv_cq	    *recv_cq;
            struct ibv_srq	    *srq;
            struct ibv_qp_cap	cap;
            enum ibv_qp_type	qp_type;
            int			        sq_sig_all;
        }; 
        struct ibv_qp_cap {
            uint32_t		max_send_wr;
            uint32_t		max_recv_wr;
            uint32_t		max_send_sge;
            uint32_t		max_recv_sge;
            uint32_t		max_inline_data;
        }
    */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_context = context->ctx;
    qp_init_attr.send_cq = context->cq;
    qp_init_attr.recv_cq = context->cq;
    qp_init_attr.srq = context->srq;
    qp_init_attr.cap.max_send_wr = 128;
    qp_init_attr.cap.max_recv_wr = 128;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_type = IBV_QPT_RC;

    for (int i = 0; i < context->num_qps; i++) {
        context->qp[i] = ibv_create_qp(context->pd, &qp_init_attr);
        if (context->qp[i] == NULL) {
            printf("|--ibv_create_qp failed.[%s]\n", strerror(errno));
        } else {
            printf("|--ibv_create_qp ok.[%d]\n", i);
        }
    }
}

static void register_memory_region(rdma_context_t* context)
{
    printf("|register_memory_regio.\n");
    // 注册一段内存区域的函数
    context->ib_buf = (char*)memalign(4096, context->ib_buf_size); // 申请一段内存
    if (context->ib_buf == NULL) {
        printf("|--memalign failed.\n");
        exit(1);
    } else {
        printf("|--memalign ok.\n");
    }

    context->mr = ibv_reg_mr(context->pd, (void*)context->ib_buf,
        context->ib_buf_size,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (context->mr == NULL) {
        printf("|--ibv_reg_mr failed.\n");
        exit(1);
    } else {
        printf("|--ibv_reg_mr ok.\n");
        printf("|----[lkey:%d][rkey:%d]\n", context->mr->lkey, context->mr->rkey);
        printf("|----[addr:%llx][length:%zu]\n", (uint64_t)context->mr->addr, context->mr->length);
    }
}

static void rdma_init(rdma_context_t* context)
{
    // 打开设备
    open_device(context);

    // 创建QP
    create_qpair(context);

    // 注册内存区域
    register_memory_region(context);
}

size_t sock_read(int sock_fd, void* buffer, size_t len)
{
    size_t nr, tot_read;
    char* buf = (char*)buffer; // avoid pointer arithmetic on void pointer
    tot_read = 0;

    while (len != 0 && (nr = read(sock_fd, buf, len)) != 0) {
        if (nr < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }
        len -= nr;
        buf += nr;
        tot_read += nr;
    }

    return tot_read;
}

size_t sock_write(int sock_fd, void* buffer, size_t len)
{
    size_t nw, tot_written;
    char* buf = (char*)buffer; // avoid pointer arithmetic on void pointer

    for (tot_written = 0; tot_written < len;) {
        nw = write(sock_fd, buf, len - tot_written);

        if (nw <= 0) {
            if (nw == -1 && errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }

        tot_written += nw;
        buf += nw;
    }
    return tot_written;
}

// Transition a QP from the RESET to INIT state
static int modify_qp_to_init(struct ibv_qp* qp)
{
    struct ibv_qp_attr attr;
    int flags;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = 1;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    return ibv_modify_qp(qp, &attr, flags);
}

// Transition a QP from the INIT to RTR state, using the specified QP number
static int modify_qp_to_rtr(struct ibv_qp* qp, uint32_t remote_qpn, uint16_t dlid, uint8_t* dgid)
{
    struct ibv_qp_attr attr;
    int flags;
    printf("[num_qp:%d][lid:%d][gid:", remote_qpn, dlid);
    for (int i = 0; i < 16; i++) {
        printf("%02x", dgid[i]);
        if (i & 1) {
            printf(":");
        }
    }
    printf("]\n");
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_256;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = 1;

    attr.ah_attr.is_global = 1;
    attr.ah_attr.port_num = 1;
    memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
    attr.ah_attr.grh.flow_label = 0;
    attr.ah_attr.grh.hop_limit = 1;
    attr.ah_attr.grh.sgid_index = 2;
    attr.ah_attr.grh.traffic_class = 0;

    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    return ibv_modify_qp(qp, &attr, flags);
}

// Transition a QP from the RTR to RTS state
static int modify_qp_to_rts(struct ibv_qp* qp)
{
    int flags;
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 12; // 18
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;

    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    return ibv_modify_qp(qp, &attr, flags);
}

static void connect_qpair(rdma_context_t* context)
{
    printf("|connect.\n");
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    ret = getaddrinfo("10.0.0.42", "4396", &hints, &result);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1) {
            continue;
        }
        ret = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            printf("|--connect ok.[%d]\n", sock_fd);
            break;
        }
        close(sock_fd);
        sock_fd = -1;
    }
    freeaddrinfo(result);

    // send
    qp_info_t* local_qp_info = (qp_info_t*)malloc(sizeof(qp_info_t));
    context->local_qp = local_qp_info;
    local_qp_info->addr = (uint64_t)context->ib_buf; // 注册内存的地址
    local_qp_info->rkey = context->mr->rkey; // 注册内存的remote key
    local_qp_info->qp_num = context->num_qps; // qp个数
    local_qp_info->lid = context->port_attr.lid;
    memcpy((void*)(&local_qp_info->gid), (void*)(&context->gid), sizeof(context->gid));
    size_t sz = sock_write(sock_fd, local_qp_info, sizeof(qp_info_t));
    printf("|--sock_write[%zu/%zu]\n", sz, sizeof(qp_info_t));
    printf("|----[addr:%llx][rkey:%d]\n", local_qp_info->addr, local_qp_info->rkey);
    printf("|----[lid:%d][qp_num:%d]\n", local_qp_info->lid, local_qp_info->qp_num);

    // recv
    qp_info_t* remote_qp_info = (qp_info_t*)malloc(sizeof(qp_info_t));
    context->remote_qp = remote_qp_info;
    sz = sock_read(sock_fd, remote_qp_info, sizeof(qp_info_t));
    printf("|--sock_read[%zu/%zu]\n", sz, sizeof(qp_info_t));
    printf("|----[addr:%llx][rkey:%d]\n", remote_qp_info->addr, remote_qp_info->rkey);
    printf("|----[lid:%d][qp_num:%d]\n", remote_qp_info->lid, remote_qp_info->qp_num);

    ret = modify_qp_to_init(context->qp[0]);
    printf("|--modify_qp_to_init = %d\n", ret);

    // modify the QP to RTR
    ret = modify_qp_to_rtr(context->qp[0], remote_qp_info->qp_num, remote_qp_info->lid, (uint8_t*)(&remote_qp_info->gid));
    printf("|--modify_qp_to_rtr = %d\n", ret);

    // modify QP state to RTS
    ret = modify_qp_to_rts(context->qp[0]);
    printf("|--modify_qp_to_rts = %d\n", ret);
}

// ------------------ Send/Recv -------------------- //
static int post_send(rdma_context_t* context, int opcode)
{
    struct ibv_sge sge;
    struct ibv_send_wr sr;
    struct ibv_send_wr* bad_wr = NULL;

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge)); // local
    memset((void*)context->ib_buf, 0xff, MSG_SIZE); // write
    sge.addr = (uintptr_t)context->ib_buf;
    sge.length = MSG_SIZE;
    sge.lkey = context->mr->lkey;

    // prepare the send work request
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = (ibv_wr_opcode)opcode;
    sr.send_flags = IBV_SEND_SIGNALED;

    qp_info_t* qp_info = context->remote_qp;
    if (opcode != IBV_WR_SEND) { // remote
        sr.wr.rdma.remote_addr = qp_info->addr;
        sr.wr.rdma.rkey = qp_info->rkey;
        printf("[local:%llx][remote:0x%llx]\n", (uint64_t)context->ib_buf, qp_info->addr);
    }

    // there is a receive request in the responder side, so we won't get any
    // into RNR flow
    int ret = ibv_post_send(context->qp[0], &sr, &bad_wr);
    if (ret) {
        printf("%s\n", strerror(ret));
    }
    return ret;
}

static int post_receive(rdma_context_t* context)
{
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr* bad_wr;
    qp_info_t* qp_info = context->remote_qp;

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)context->ib_buf;
    sge.length = MSG_SIZE;
    sge.lkey = context->mr->lkey;

    // prepare the receive work request
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;

    // post the receive request to the RQ
    int ret = ibv_post_recv(context->qp[0], &rr, &bad_wr);
    if (ret) {
        printf("%s\n", strerror(ret));
    }
    return ret;
}

static void poll_cq(rdma_context_t* context)
{
    int num_wc = 20;
    struct ibv_qp** qp = context->qp;
    struct ibv_cq* cq = context->cq;
    struct ibv_srq* srq = context->srq;
    struct ibv_wc* wc = NULL;
    wc = (struct ibv_wc*)calloc(num_wc, sizeof(struct ibv_wc));

    while (true) {
        int n = ibv_poll_cq(cq, num_wc, wc);
        if (n < 0) {
            printf("ibv_poll_cq failed.\n");
            exit(1);
        }
        if (n) {
            printf("[addr:%llx][data:%llx]\n", (uint64_t*)context->local_qp->addr, *(uint64_t*)context->local_qp->addr);
            printf("%d\n", n);
        }
    }
}

int main(int argc, char** argv)
{
    rdma_context_t _ctx;
    memset(&_ctx, 0, sizeof(_ctx));
    _ctx.num_qps = 1;
    _ctx.ib_buf_size = 2UL * 1024 * 1024;

    rdma_init(&_ctx);
    connect_qpair(&_ctx);

    int ret;
    ret = post_send(&_ctx, IBV_WR_RDMA_READ);
    printf("post_send = %d\n", ret);

#if 0
    int tmp;
    printf("[addr:%llx][data:%llx]\n", (uint64_t*)_ctx.local_qp->addr, *(uint64_t*)_ctx.local_qp->addr);
    while (true) {
        scanf("%d", &tmp);
        if (tmp) {
            printf("[addr:%llx][data:%llx]\n", (uint64_t*)_ctx.local_qp->addr, *(uint64_t*)_ctx.local_qp->addr);
        } else {
            break;
        }
    }
#endif

#if 0
    ret = post_receive(&_ctx);
    printf("post_receive = %d\n", ret);
    if (!ret) {
        printf("data = %llu\n", *(uint64_t*)_ctx.remote_qp->addr);
    }
#endif
    poll_cq(&_ctx);
    return 0;
}