/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-20 12:06:23
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /rdma_demo/hello_rdma.cc
 */

#include <errno.h>
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
    char* ib_buf;

public: // need initlizate
    int num_qps;
    size_t ib_buf_size;
};

rdma_context_t g_context;

static void open_device(rdma_context_t* context)
{
    int _res;
    int _num_dev = 0;
    struct ibv_device* _dev = NULL;
    struct ibv_device** _dev_list = NULL;

    // 1.1 获得RDMA列表
    _dev_list = ibv_get_device_list(&_num_dev);
    if (_num_dev == 0) {
        printf("ibv_get_device_list failed.\n");
        exit(1);
    } else {
        printf("ibv_get_device_list ok.[%d]\n", _num_dev);
        _dev = *_dev_list; // used first
    }

    // 1.2 打开RDMA设备
    context->ctx = ibv_open_device(_dev);
    if (context->ctx == NULL) {
        printf("ibv_open_device failed.\n");
        exit(2);
    } else {
        printf("ibv_open_device ok.\n");
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
        printf("ibv_query_port failed.\n");
    } else {
        printf("ibv_query_port ok.\n");
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
        printf("ibv_query_device failed.\n");
    } else {
        printf("ibv_query_device ok.\n");
        printf("[VERSION:%d]\n", context->dev_attr.hw_ver);
        printf("[MAX_NUM_CQ:%d][MAX_CQE]\n", context->dev_attr.max_cq, context->dev_attr.max_cqe);
        printf("[MAX_NUM_MR:%dMB][MR_SIZE:%lluGB]\n", context->dev_attr.max_mr / (1024 * 1024), context->dev_attr.max_mr_size / (1024UL * 1024 * 1024));
    }
}

static void create_qpair(rdma_context_t* context)
{
    // create protection domain (pd)
    // protection domain可以看作是一个内存保护单位，在内存区域和队列直接建立一个关联关系，防止未授权的访问。
    context->pd = ibv_alloc_pd(context->ctx);
    if (context->pd == NULL) {
        printf("ibv_alloc_pd failed.\n");
        exit(1);
    } else {
        printf("ibv_alloc_pd ok.\n");
    }

    // create completion queue (cq)
    context->cq = ibv_create_cq(context->ctx, context->dev_attr.max_cqe, NULL, NULL, 0);
    if (context->cq == NULL) {
        printf("ibv_create_cq failed.\n");
        exit(1);
    } else {
        printf("ibv_create_cq ok.\n");
    }

    // create shared received queue (srq)
    struct ibv_srq_init_attr srq_init_attr;
    srq_init_attr.attr.max_wr = context->dev_attr.max_srq_wr;
    srq_init_attr.attr.max_sge = 1;
    context->srq = ibv_create_srq(context->pd, &srq_init_attr);
    if (context->srq == NULL) {
        printf("ibv_create_srq failed.\n");
    } else {
        printf("ibv_create_srq ok.\n");
    }

    struct ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_context = context->ctx;
    qp_init_attr.send_cq = context->cq;
    qp_init_attr.recv_cq = context->cq;
    qp_init_attr.srq = context->srq;
    qp_init_attr.cap.max_send_wr = context->dev_attr.max_qp_wr;
    qp_init_attr.cap.max_recv_wr = context->dev_attr.max_qp_wr;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_type = IBV_QPT_RC;

    printf("%s\n", strerror(errno));
    context->qp = (struct ibv_qp**)calloc(context->num_qps, sizeof(struct ibv_qp*));
    for (int i = 0; i < context->num_qps; i++) {
        context->qp[i] = ibv_create_qp(context->pd, &qp_init_attr);
        if (context->qp[i] == NULL) {
            printf("ibv_create_qp failed.[%s]\n", strerror(errno));
        } else {
            printf("ibv_create_qp ok.[%d]\n", i);
        }
    }
}

static void register_memory_region(rdma_context_t* context)
{
    // 注册一段内存区域的函数
    context->ib_buf = (char*)memalign(4096, context->ib_buf_size); // 申请一段内存
    if (context->ib_buf == NULL) {
        printf("memalign failed.\n");
        exit(1);
    } else {
        printf("memalign ok.\n");
    }

    context->mr = ibv_reg_mr(context->pd, (void*)context->ib_buf,
        context->ib_buf_size,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (context->mr == NULL) {
        printf("ibv_reg_mr failed.\n");
        exit(1);
    } else {
        printf("ibv_reg_mr ok.\n");
    }
}

static void do_recv(rdma_context_t* context)
{
}

static void do_send()
{
}

int main(int argc, char** argv)
{
    rdma_context_t _ctx;
    memset(&_ctx, 0, sizeof(_ctx));
    _ctx.num_qps = 4;
    _ctx.ib_buf_size = 2UL * 1024 * 1024;

    open_device(&_ctx);
    create_qpair(&_ctx);
    register_memory_region(&_ctx);
    return 0;
}