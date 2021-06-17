/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-17 14:40:51
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /rdma_demo/hello_rdma.cc
 */

#include <infiniband/verbs.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

struct IBRes {
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

int main(int argc, char** argv)
{
    printf("Hello RDMA!\n");

    int num_dev = 0;
    struct ibv_device** dev_list = NULL;
    struct ibv_device* dev;

    // 获得设备列表
    dev_list = ibv_get_device_list(&num_dev);
    if (num_dev == 0) {
        printf("Failed to get ib device list! [%d]\n", num_dev);
        exit(1);
    } else {
        printf("Get ib device list! [%d]\n", num_dev);
        dev = *dev_list;
    }

    IBRes _ib;
    memset(&_ib, 0, sizeof(_ib));

    // 打开某个设备，返回其上下文
    _ib.ctx = ibv_open_device(dev);
    if (_ib.ctx == NULL) {
        printf("Failed to open ib device!\n");
        exit(2);
    }

    // 创建一个保护域，protection domain。
    // protection domain可以看作是一个内存保护单位，在内存区域和队列直接建立一个关联关系，防止未授权的访问。
    _ib.pd = ibv_alloc_pd(_ib.ctx);

    // 返回RDMA设备上下文（context）的端口的属性。
    ibv_query_port(_ib.ctx, 1, &_ib.port_attr);

    // 注册一段内存区域的函数
    _ib.ib_buf_size = 2UL * 1024 * 1024;
    _ib.ib_buf = (char*)memalign(4096, _ib.ib_buf_size); // 申请一段内存
    _ib.mr = ibv_reg_mr(_ib.pd, (void*)_ib.ib_buf,
        _ib.ib_buf_size,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);

    // 查询设备，获得设备属性
    ibv_query_device(_ib.ctx, &_ib.dev_attr);

    // 为RDMA设备上下文创建完成队列
    _ib.cq = ibv_create_cq(_ib.ctx, _ib.dev_attr.max_cqe, NULL, NULL, 0);

    /* create srq */
    struct ibv_srq_init_attr srq_init_attr;
    srq_init_attr.attr.max_wr = _ib.dev_attr.max_srq_wr;
    srq_init_attrattr.max_sge = 1;
    _ib.srq = ibv_create_srq(_ib.pd, &srq_init_attr);
    return 0;
}