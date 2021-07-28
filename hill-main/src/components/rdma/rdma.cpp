#include "rdma.hpp"
namespace Hill {
    auto RDMA::make_rdma(std::string &dev_name, int ib_port, int gid_idx) -> std::pair<RDMAPtr, RDMAStatus> {
        int dev_num = 0;
        struct ibv_device **devices = ibv_get_device_list(&dev_num);
        if (!devices) {
            return std::make_pair(nullptr, RDMAStatus::NoRDMADeviceList);
        }

        for (int i = 0; i < dev_num; i++) {
            if (dev_name.compare(ibv_get_device_name(devices[i])) == 0) {
                if (auto ctx = ibv_open_device(devices[i]); ctx) {
                    return std::make_pair(std::make_unique<RDMA>(ctx, ib_port, gid_idx), RDMAStatus::Ok);
                }
            }
        }
        return std::make_pair(nullptr, RDMAStatus::DeviceNotFound);
    }

    auto RDMA::open(void *membuf, size_t memsize, size_t cqe, int mr_access, struct ibv_qp_init_attr &attr) -> RDMAStatus {
        if (!membuf || !cqe) {
            return RDMAStatus::InvalidArguments;
        }

        if (!(pd = ibv_alloc_pd(ctx))) {
            return RDMAStatus::CannotAllocPD;
        }

        if (!(cq = ibv_create_cq(ctx, cqe, nullptr, nullptr, 0))) {
            return RDMAStatus::CannotCreateCQ;
        }

        if (!(mr = ibv_reg_mr(pd, membuf, memsize, mr_access))) {
            return RDMAStatus::CannotRegMR;
        }

        attr.send_cq = cq;
        attr.recv_cq = cq;
        if (!(qp = ibv_create_qp(pd, &attr))) {
            return RDMAStatus::CannotCreateQP;
        }

        union ibv_gid my_gid;
        if (gid_idx >= 0) {
            if (ibv_query_gid(ctx, ib_port, gid_idx, &my_gid)) {
                return RDMAStatus::NoGID;
            }
            memcpy(local.gid, &my_gid, 16);
        }
        local.addr = (uint64_t)membuf;
        local.rkey = mr->rkey;
        local.qp_num = qp->qp_num;

        struct ibv_port_attr pattr;
        if (ibv_query_port(ctx, ib_port, &pattr)) {
            return RDMAStatus::CannotQueryPort;
        }
        local.lid = pattr.lid;

        buf = membuf;
        return RDMAStatus::Ok;
    }

    auto RDMA::get_default_qp_init_attr(const int ib_port) -> std::unique_ptr<struct ibv_qp_attr> {
        auto attr = std::make_unique<struct ibv_qp_attr>();
        memset(attr.get(), 0, sizeof(struct ibv_qp_attr));

        attr->qp_state = IBV_QPS_INIT;
        attr->port_num = ib_port;
        attr->pkey_index = 0;
        attr->qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
        return attr;
    }

    auto RDMA::get_default_qp_rtr_attr(const connection_certificate &remote,
                                       const int ib_port = 1,
                                       const int sgid_idx = -1) -> std::unique_ptr<struct ibv_qp_attr> {
        auto attr = std::make_unique<struct ibv_qp_attr>();
        memset(attr.get(), 0, sizeof(struct ibv_qp_attr));
        
        attr->qp_state = IBV_QPS_RTR;
        attr->path_mtu = IBV_MTU_256;
        attr->dest_qp_num = remote.qp_num;
        attr->rq_psn = 0;
        attr->max_dest_rd_atomic = 1;
        attr->min_rnr_timer = 0x12;

        attr->ah_attr.is_global = 0;
        attr->ah_attr.dlid = remote.lid;
        attr->ah_attr.sl = 0;
        attr->ah_attr.src_path_bits = 0;
        attr->ah_attr.port_num = ib_port;
        
        if (sgid_idx >= 0) {
            attr->ah_attr.is_global = 1;
            attr->ah_attr.port_num = 1;
            memcpy(&attr->ah_attr.grh.dgid, remote.gid, 16);
            attr->ah_attr.grh.flow_label = 0;
            attr->ah_attr.grh.hop_limit = 1;
            attr->ah_attr.grh.sgid_index = sgid_idx;
            attr->ah_attr.grh.traffic_class = 0;
        }
        return attr;
    }

    auto RDMA::get_default_qp_rts_attr() -> std::unique_ptr<struct ibv_qp_attr> {
        auto attr = std::make_unique<struct ibv_qp_attr>();
        memset(attr.get(), 0, sizeof(struct ibv_qp_attr));
        
        attr->qp_state = IBV_QPS_RTS;
        attr->timeout = 0x12; // 18
        attr->retry_cnt = 6;
        attr->rnr_retry = 0;
        attr->sq_psn = 0;
        attr->max_rd_atomic = 1;
        return attr;
    }

    auto RDMA::modify_qp(struct ibv_qp_attr &attr, int mask) noexcept -> std::pair<RDMAStatus, int> {
        if (!is_opened()) {
            return std::make_pair(RDMAStatus::DeviceNotOpened, -1);
        }
        
        if (auto ret =  ibv_modify_qp(qp, &attr, mask); ret == 0) {
            return std::make_pair(RDMAStatus::Ok, ret);
        } else {
            return std::make_pair(RDMAStatus::CannotInitQP, ret);
        }
    }

    auto RDMA::exchange_certificate(int sockfd) noexcept -> RDMAStatus {
        if (!is_opened())
            return RDMAStatus::DeviceNotOpened;
        
        const int normal = sizeof(connection_certificate);
        connection_certificate tmp;
        tmp.addr = htonll(local.addr);
        tmp.rkey = htonl(local.rkey);
        tmp.qp_num = htonl(local.qp_num);
        tmp.lid = htons(local.lid);
        memcpy(tmp.gid, local.gid, 16);
        if (write(sockfd, &tmp, normal) != normal) {
            return RDMAStatus::WriteError;
        }

        if (read(sockfd, &tmp, normal) != normal) {
            return RDMAStatus::ReadError;
        }

        remote.addr = ntohll(tmp.addr);
        remote.rkey = ntohl(tmp.rkey);
        remote.qp_num = ntohl(tmp.qp_num);
        remote.lid = ntohs(tmp.lid);
        memcpy(remote.gid, tmp.gid, 16);
        return RDMAStatus::Ok;
    }

    auto RDMA::post_send_helper(uint8_t *msg, size_t msg_len, enum ibv_wr_opcode opcode,
                                size_t offset) -> std::pair<RDMAStatus, int> {
        struct ibv_sge sg;
        struct ibv_send_wr sr;
        struct ibv_send_wr *bad_wr;

        if (msg) {
            memcpy(buf, msg, msg_len);
        }
        
        memset(&sg, 0, sizeof(sg));
        sg.addr	  = (uintptr_t)buf;
        sg.length = msg_len;
        sg.lkey	  = mr->lkey;
 
        memset(&sr, 0, sizeof(sr));
        sr.wr_id      = 0;
        sr.sg_list    = &sg;
        sr.num_sge    = 1;
        sr.opcode     = opcode;
        sr.send_flags = IBV_SEND_SIGNALED;

        if (opcode != IBV_WR_SEND) {
            sr.wr.rdma.remote_addr = remote.addr + offset;
            sr.wr.rdma.rkey = remote.rkey;
        }
 
        if (auto ret = ibv_post_send(qp, &sr, &bad_wr); ret != 0) {
            return std::make_pair(RDMAStatus::PostFailed, ret);
        }
        return std::make_pair(RDMAStatus::Ok, 0);
    }

    auto RDMA::post_send(uint8_t *msg, size_t msg_len, size_t offset) -> std::pair<RDMAStatus, int> {
        return post_send_helper(msg, msg_len, IBV_WR_SEND, offset);
    }
    
    auto RDMA::post_read(size_t msg_len, size_t offset) -> std::pair<RDMAStatus, int> {
        return post_send_helper(nullptr, msg_len, IBV_WR_RDMA_READ, offset);
    }
    
    auto RDMA::post_write(uint8_t *msg, size_t msg_len, size_t offset) -> std::pair<RDMAStatus, int> {
        return post_send_helper(msg, msg_len, IBV_WR_RDMA_WRITE, offset);
    }

    auto RDMA::post_recv(size_t msg_len, size_t offset) -> std::pair<RDMAStatus, int> {
        struct ibv_sge sg;
        struct ibv_recv_wr wr;
        struct ibv_recv_wr *bad_wr;

        auto tmp = (uint8_t *)buf + offset;
        memset(&sg, 0, sizeof(sg));
        sg.addr	  = (uintptr_t)tmp;
        sg.length = msg_len;
        sg.lkey	  = mr->lkey;
 
        memset(&wr, 0, sizeof(wr));
        wr.wr_id      = 0;
        wr.sg_list    = &sg;
        wr.num_sge    = 1;
 
        if (auto ret = ibv_post_recv(qp, &wr, &bad_wr); ret != 0) {
            return std::make_pair(RDMAStatus::RecvFailed, ret);
        }
        return std::make_pair(RDMAStatus::Ok, 0);
    }

    auto RDMA::poll_completion() noexcept -> int {
        struct ibv_wc wc;
        int ret;
        do {
            ret = ibv_poll_cq(cq, 1, &wc);
        } while (ret == 0);

        return ret;
    }

    auto RDMA::fill_buf(uint8_t *msg, size_t msg_len, size_t offset) -> void{
        memcpy((uint8_t *)buf + offset, msg, msg_len);
    }
}
