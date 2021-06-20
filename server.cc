/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-20 20:44:50
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

public: // need initlizate
    int num_qps;
    size_t ib_buf_size;
};

struct qp_info_t {
    uint32_t lid;
    uint32_t qp_num;
    uint32_t rank;
    uint32_t unused_;
};

rdma_context_t g_context;

static void open_device(rdma_context_t* context)
{
    printf("|open_device.\n");
    int _res;
    int _num_dev = 0;
    struct ibv_device* _dev = NULL;
    struct ibv_device** _dev_list = NULL;

    // 1.1 获得RDMA列表
    _dev_list = ibv_get_device_list(&_num_dev);
    if (_num_dev == 0) {
        printf("|--ibv_get_device_list failed.\n");
        exit(1);
    } else {
        printf("|--ibv_get_device_list ok.[%d]\n", _num_dev);
        _dev = *_dev_list; // used first
    }

    // 1.2 打开RDMA设备
    context->ctx = ibv_open_device(_dev);
    if (context->ctx == NULL) {
        printf("|--ibv_open_device failed.\n");
        exit(2);
    } else {
        printf("|--ibv_open_device ok.\n");
    }

    // 1.3 返回RDMA设备的端口的属性。
    // struct ibv_port_attr {
    //            enum ibv_port_state     state;          /* Logical port state */
    //            enum ibv_mtu            max_mtu;        /* Max MTU supported by port */
    //            enum ibv_mtu            active_mtu;     /* Actual MTU */
    //            int                     gid_tbl_len;    /* Length of source GID table */
    //            uint32_t                port_cap_flags; /* Port capabilities */
    //            uint32_t                max_msg_sz;     /* Maximum message size */
    //            uint32_t                bad_pkey_cntr;  /* Bad P_Key counter */
    //            uint32_t                qkey_viol_cntr; /* Q_Key violation counter */
    //            uint16_t                pkey_tbl_len;   /* Length of partition table */
    //            uint16_t                lid;            /* Base port LID */
    //            uint16_t                sm_lid;         /* SM LID */
    //            uint8_t                 lmc;            /* LMC of LID */
    //            uint8_t                 max_vl_num;     /* Maximum number of VLs */
    //            uint8_t                 sm_sl;          /* SM service level */
    //            uint8_t                 subnet_timeout; /* Subnet propagation delay */
    //            uint8_t                 init_type_reply;/* Type of initialization performed by SM */
    //            uint8_t                 active_width;   /* Currently active link width */
    //            uint8_t                 active_speed;   /* Currently active link speed */
    //            uint8_t                 phys_state;     /* Physical port state */
    //            uint8_t                 link_layer;     /* link layer protocol of the port */
    //            uint8_t                 flags;          /* Port flags */
    //            uint16_t                port_cap_flags2;/* Port capabilities */
    // };
    _res = ibv_query_port(context->ctx, 1, &context->port_attr);
    if (_res) {
        printf("|--ibv_query_port failed.\n");
    } else {
        printf("|--ibv_query_port ok.\n");
    }

    // 1.4 查询设备获得设备属性
    // struct ibv_device_attr {
    //           char                    fw_ver[64];             /* FW version */
    //           uint64_t                node_guid;              /* Node GUID (in network byte order) */
    //           uint64_t                sys_image_guid;         /* System image GUID (in network byte order) */
    //           uint64_t                max_mr_size;            /* Largest contiguous block that can be registered */
    //           uint64_t                page_size_cap;          /* Supported memory shift sizes */
    //           uint32_t                vendor_id;              /* Vendor ID, per IEEE */
    //           uint32_t                vendor_part_id;         /* Vendor supplied part ID */
    //           uint32_t                hw_ver;                 /* Hardware version */
    //           int                     max_qp;                 /* Maximum number of supported QPs */
    //           int                     max_qp_wr;              /* Maximum number of outstanding WR on any work queue */
    //           unsigned int            device_cap_flags;       /* HCA capabilities mask */
    //           int                     max_sge;                /* Maximum number of s/g per WR for SQ & RQ of QP for non RDMA Read operations */
    //           int                     max_sge_rd;             /* Maximum number of s/g per WR for RDMA Read operations */
    //           int                     max_cq;                 /* Maximum number of supported CQs */
    //           int                     max_cqe;                /* Maximum number of CQE capacity per CQ */
    //           int                     max_mr;                 /* Maximum number of supported MRs */
    //           int                     max_pd;                 /* Maximum number of supported PDs */
    //           int                     max_qp_rd_atom;         /* Maximum number of RDMA Read & Atomic operations that can be outstanding per QP */
    //           int                     max_ee_rd_atom;         /* Maximum number of RDMA Read & Atomic operations that can be outstanding per EEC */
    //           int                     max_res_rd_atom;        /* Maximum number of resources used for RDMA Read & Atomic operations by this HCA as the Target */
    //           int                     max_qp_init_rd_atom;    /* Maximum depth per QP for initiation of RDMA Read & Atomic operations */
    //           int                     max_ee_init_rd_atom;    /* Maximum depth per EEC for initiation of RDMA Read & Atomic operations */
    //           enum ibv_atomic_cap     atomic_cap;             /* Atomic operations support level */
    //           int                     max_ee;                 /* Maximum number of supported EE contexts */
    //           int                     max_rdd;                /* Maximum number of supported RD domains */
    //           int                     max_mw;                 /* Maximum number of supported MWs */
    //           int                     max_raw_ipv6_qp;        /* Maximum number of supported raw IPv6 datagram QPs */
    //           int                     max_raw_ethy_qp;        /* Maximum number of supported Ethertype datagram QPs */
    //           int                     max_mcast_grp;          /* Maximum number of supported multicast groups */
    //           int                     max_mcast_qp_attach;    /* Maximum number of QPs per multicast group which can be attached */
    //           int                     max_total_mcast_qp_attach;/* Maximum number of QPs which can be attached to multicast groups */
    //           int                     max_ah;                 /* Maximum number of supported address handles */
    //           int                     max_fmr;                /* Maximum number of supported FMRs */
    //           int                     max_map_per_fmr;        /* Maximum number of (re)maps per FMR before an unmap operation in required */
    //           int                     max_srq;                /* Maximum number of supported SRQs */
    //           int                     max_srq_wr;             /* Maximum number of WRs per SRQ */
    //           int                     max_srq_sge;            /* Maximum number of s/g per SRQ */
    //           uint16_t                max_pkeys;              /* Maximum number of partitions */
    //           uint8_t                 local_ca_ack_delay;     /* Local CA ack delay */
    //           uint8_t                 phys_port_cnt;          /* Number of physical ports */
    // };
    _res = ibv_query_device(context->ctx, &context->dev_attr);
    if (_res) {
        printf("|--ibv_query_device failed.\n");
    } else {
        printf("|--ibv_query_device ok.\n");
        printf("|----[VERSION:%d]\n", context->dev_attr.hw_ver);
        printf("|----[MAX_NUM_CQ:%d][MAX_CQE:%d]\n", context->dev_attr.max_cq, context->dev_attr.max_cqe);
        printf("|----[MAX_QP_WR:%d]\n", context->dev_attr.max_qp_wr);
        printf("|----[MAX_NUM_MR:%dMB][MR_SIZE:%lluGB]\n", context->dev_attr.max_mr / (1024 * 1024), context->dev_attr.max_mr_size / (1024UL * 1024 * 1024));
    }
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
    // qp_init_attr.qp_context = context->ctx;
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

    /*
    struct ibv_mr {
        struct ibv_context  *context;
        struct ibv_pd	    *pd;
        void		        *addr;
        size_t			    length;
        uint32_t		    handle;
        uint32_t		    lkey;
        uint32_t		    rkey;
    };
    */
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

////////////////////// socket ///////////////////////
//          使用传统的TCP/IP链接去交换信息             //
/////////////////////////////////////////////////////
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

int modify_qp_to_rts(struct ibv_qp* qp, uint32_t target_qp_num, uint16_t target_lid)
{
    int ret = 0;
    /* change QP state to INIT */
    {
        struct ibv_qp_attr qp_attr;
        qp_attr.qp_state = IBV_QPS_INIT;
        qp_attr.pkey_index = 0;
        qp_attr.port_num = 1;
        qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE;
        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
        if (!ret) {
            printf("ibv_modify_qp ok.\n");
        } else {
            printf("ibv_modify_qp failed.\n");
            exit(1);
        }
    }

    /* Change QP state to RTR */
    {
        struct ibv_qp_attr qp_attr;
        qp_attr.qp_state = IBV_QPS_RTR;
        qp_attr.path_mtu = IBV_MTU_4096;
        qp_attr.dest_qp_num = target_qp_num;
        qp_attr.rq_psn = 0;
        qp_attr.max_dest_rd_atomic = 1;
        qp_attr.min_rnr_timer = 12;
        qp_attr.ah_attr.is_global = 0;
        qp_attr.ah_attr.dlid = target_lid;
        qp_attr.ah_attr.sl = 0;
        qp_attr.ah_attr.src_path_bits = 0;
        qp_attr.ah_attr.port_num = 1;
        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
        if (!ret) {
            printf("ibv_modify_qp ok.\n");
        } else {
            printf("ibv_modify_qp failed.\n");
            exit(1);
        }
    }

    /* Change QP state to RTS */
    {
        struct ibv_qp_attr qp_attr;
        qp_attr.qp_state = IBV_QPS_RTS;
        qp_attr.timeout = 14;
        qp_attr.retry_cnt = 7;
        qp_attr.rnr_retry = 7;
        qp_attr.sq_psn = 0;
        qp_attr.max_rd_atomic = 1;
        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
        if (!ret) {
            printf("ibv_modify_qp ok.\n");
        } else {
            printf("ibv_modify_qp failed.\n");
            exit(1);
        }
    }
    return 0;
}

static void connect(rdma_context_t* context)
{
    printf("|connect.\n");
    /////////////////// bind /////////////////////
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(NULL, "4396", &hints, &result);
    if (!ret) {
        printf("|--getaddrinfo ok.\n");
    } else {
        printf("|--getaddrinfo failed.\n");
        exit(1);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd < 0) {
            continue;
        }
        ret = bind(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            break;
        }
        close(sock_fd);
        sock_fd = -1;
    }
    freeaddrinfo(result);

    /////////////////// listen /////////////////////
    ret = listen(sock_fd, 5);
    if (!ret) {
        printf("|--listen ok.\n");
    } else {
        printf("|--listen failed.\n");
        exit(1);
    }

    /////////////////// accept /////////////////////
    int peer_sockfd;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);
    peer_sockfd = accept(sock_fd, (struct sockaddr*)&peer_addr, &peer_addr_len);
    printf("|--accept ok.[%d]\n", peer_sockfd);

    // recv
    qp_info_t remote_qp_info;
    size_t sz = sock_read(peer_sockfd, &remote_qp_info, sizeof(remote_qp_info));
    printf("[%zu/%zu]\n", sz, sizeof(remote_qp_info));
    printf("[lid:%d][qp_num:%d][rank:%d]\n", remote_qp_info.lid, remote_qp_info.qp_num, remote_qp_info.rank);

    // send
    qp_info_t local_qp_info;
    local_qp_info.lid = context->port_attr.lid;
    local_qp_info.qp_num = context->num_qps;
    local_qp_info.rank = 1;
    sz = sock_write(peer_sockfd, &local_qp_info, sizeof(local_qp_info));
    printf("[%zu/%zu]\n", sz, sizeof(local_qp_info));

    modify_qp_to_rts(context->qp[0], 1, remote_qp_info.lid);
}

static void register_recv_wq(rdma_context_t* context)
{
    printf("|register_recv_wq.\n");
    int ret = 0;
    struct ibv_recv_wr* bad_recv_wr;

    /*
    struct ibv_sge {
        uint64_t addr; // buffer address
        uint32_t length; // buffer length
        uint32_t lkey; // ???
    };
    */
    struct ibv_sge list;
    list.addr = (uint64_t)context->ib_buf;
    list.length = context->ib_buf_size;
    list.lkey = context->mr->lkey;

    /*
    struct ibv_recv_wr {
        uint64_t wr_id;  // work request id
        struct ibv_recv_wr* next; // next work request
        struct ibv_sge* sg_list;
        int num_sge;
    };
    */
    struct ibv_recv_wr recv_wr;
    recv_wr.wr_id = 1;
    recv_wr.sg_list = &list;
    recv_wr.num_sge = 1;

    ret = ibv_post_srq_recv(context->srq, &recv_wr, &bad_recv_wr);
    if (!ret) {
        printf("|--ibv_post_srq_recv ok.\n");
    } else {
        printf("|--ibv_post_srq_recv failed.\n");
        exit(1);
    }
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
        if (n) {
            printf("%d\n", n);
        }
    }
}

////////////////////// socket ///////////////////////
//                     MAIN                        //
/////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    rdma_context_t _ctx;
    memset(&_ctx, 0, sizeof(_ctx));
    _ctx.num_qps = 1;
    _ctx.ib_buf_size = 2UL * 1024 * 1024;

    open_device(&_ctx);
    create_qpair(&_ctx);
    register_memory_region(&_ctx);
    connect(&_ctx);
    register_recv_wq(&_ctx);
    poll_cq(&_ctx);
    return 0;
}