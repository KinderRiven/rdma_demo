/*
 * rdma.hpp/cpp:
 *   A wrapper organizing ibv_context, ibv_pd, ibv_cq, ibv_mr, ibv_qp and some other customized data structure for a RDMA RC connection together.
 * 
 * How to use:
 *    1. auto [rdma, status] = RDMA::make_rdma()
 *    2. auto status = rdma->open()
 *    3. rdma->exchange_certificate(socket)
 *    4. rdma->modify_qp(init_attr, init_mask)
 *    5. rdma->modify_qp(rtr_attr, rtr_mask)
 *    6. rdma->modify_qp(rts_attr, rts_mask)
 *    7. rdma->post_send/recv/read/write
 *    8. rdma->poll_complection
 */


#ifndef __HILL__RDMA__RDMA__
#define __HILL__RDMA__RDMA__
#include <memory>
#include <optional>
#include <functional>
#include <infiniband/verbs.h>
#include <byteswap.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif


namespace Hill {
    struct connection_certificate {
        uint64_t addr;   // registered memory address
        uint32_t rkey;   // remote key
        uint32_t qp_num; // local queue pair number
        uint16_t lid;    // LID of the ib port
        uint8_t gid[16]; // mandatory for RoCE
    } __attribute__((packed));
    
    enum class RDMAStatus {
        Ok,
        NoRDMADeviceList,        
        DeviceNotFound,
        DeviceNotOpened,
        NoGID,
        CannotOpenDevice,
        CannotAllocPD,
        CannotCreateCQ,
        CannotRegMR,
        CannotCreateQP,
        CannotQueryPort,
        InvalidGIDIdx,
        InvalidIBPort,
        InvalidArguments,
        CannotInitQP,
        QPRTRFailed,
        QPRTSFailed,
        ReadError,
        WriteError,
        PostFailed,
        RecvFailed,
    };
    
    class RDMA {
    private:
        struct ibv_context *ctx;
        struct ibv_pd *pd;        
        struct ibv_cq *cq;
        struct ibv_mr *mr;
        struct ibv_qp *qp;
        connection_certificate local, remote;
        int ib_port;
        int gid_idx;        
        std::string dev_name;
        void *buf;
        
        auto post_send_helper(uint8_t *msg, size_t msg_len, enum ibv_wr_opcode opcode, size_t offset) -> std::pair<RDMAStatus, int>;
        
    public:
        using RDMAPtr = std::unique_ptr<RDMA>;
        static auto make_rdma(std::string &dev_name, int ib_port, int gid_idx) -> std::pair<RDMAPtr, RDMAStatus>;

        // never explicitly instantiated
        RDMA() = delete;
        RDMA(const RDMA &) = delete;
        RDMA(RDMA &&) = delete;
        RDMA &operator=(const RDMA &) = delete;
        RDMA &operator=(RDMA&&) = delete;
        RDMA(struct ibv_context *ctx_, int ib_port_, int gid_idx_) :
            ctx(ctx_), pd(nullptr), cq(nullptr), mr(nullptr), qp(nullptr), ib_port(ib_port_), gid_idx(gid_idx_),
            buf(nullptr) {
            memset(&local, 0, sizeof(connection_certificate));
            memset(&remote, 0, sizeof(connection_certificate));            
        };
        ~RDMA() {
            if (qp) ibv_destroy_qp(qp);
            if (mr) ibv_dereg_mr(mr);
            if (cq) ibv_destroy_cq(cq);
            if (pd) ibv_dealloc_pd(pd);
            if (ctx) ibv_close_device(ctx);
        }

        // Public APIs
        // following methods are for read-only purposes
        inline auto get_ctx() const noexcept -> const struct ibv_context * {
            return ctx;
        }
        
        inline auto get_pd() const noexcept -> const struct ibv_pd * {
            return pd;
        }
        
        inline auto get_cq() const noexcept -> const struct ibv_cq * {
            return cq;
        }
        
        inline auto get_mr() const noexcept -> const struct ibv_mr * {
            return mr;
        }
        
        inline auto get_qp() const noexcept -> const struct ibv_qp * {
            return qp;
        }
        
        inline auto get_local() const noexcept -> const connection_certificate & {
            return local;
        }
        
        inline auto get_remote() const noexcept -> const connection_certificate & {
            return remote;
        }

        inline auto get_ib_port() const noexcept -> const int & {
            return ib_port;
        }

        inline auto get_gid_idx() const noexcept -> const int & {
            return gid_idx;
        }

        inline auto get_dev_name() const noexcept -> const std::string & {
            return dev_name;            
        }

        inline auto get_buf() const noexcept -> const void * {
            return buf;
        }

        inline auto get_char_buf() const noexcept -> const char * {
            return (char *)buf;
        }
        
        
        /*
          Open an initialized RDMA device made from `make_rdma`
          @membuf: memory region to be registered
          @memsize: memory region size
          @cqe: completion queue capacity
          @attr: queue pair initialization attribute.
                 No need to fill the `send_cq` and `recv_cq` fields, they are filled automatically
        */
        auto open(void *membuf, size_t memsize, size_t cqe, int mr_access, struct ibv_qp_init_attr &attr) -> RDMAStatus;

        inline auto ctx_valid() const noexcept -> bool {
            return ctx;
        }

        inline auto pd_valid() const noexcept -> bool {
            return pd;
        }

        inline auto cq_valid() const noexcept -> bool {
            return cq;
        }

        inline auto mr_valid() const noexcept -> bool {
            return mr;
        }

        inline auto qp_valid() const noexcept -> bool {
            return qp;
        }

        inline auto is_opened() const noexcept -> bool {
            return buf;
        }

        inline auto connectable() const noexcept -> bool {
            return buf;
        }
        
        static auto get_default_qp_init_attr(const int ib_port = 1) -> std::unique_ptr<struct ibv_qp_attr>;
        inline static auto get_default_qp_init_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
        }
        
        static auto get_default_qp_rtr_attr(const connection_certificate &remote,
                                            const int ib_port,
                                            const int sgid_idx) -> std::unique_ptr<struct ibv_qp_attr>;
        static auto get_default_qp_rtr_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
        }
        
        static auto get_default_qp_rts_attr() -> std::unique_ptr<struct ibv_qp_attr>;
        static auto get_default_qp_rts_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
        }
        
        auto modify_qp(struct ibv_qp_attr &, int mask) noexcept -> std::pair<RDMAStatus, int>;
        auto exchange_certificate(int sockfd) noexcept -> RDMAStatus;

        auto post_send(uint8_t *msg, size_t msg_len, size_t offset = 0) -> std::pair<RDMAStatus, int>;
        auto post_read(size_t msg_len, size_t offset = 0) -> std::pair<RDMAStatus, int>;
        auto post_write(uint8_t *msg, size_t msg_len, size_t offset = 0) -> std::pair<RDMAStatus, int>;
        auto post_recv(size_t msg_len, size_t offset = 0) -> std::pair<RDMAStatus, int>;
        auto poll_completion() noexcept -> int;
        auto fill_buf(uint8_t *msg, size_t msg_len, size_t offset = 0) -> void;
    };
}
#endif
