/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-17 11:29:58
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /rdma_demo/hello_rdma.cc
 */
#include <infiniband/verbs.h>
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

    struct ibv_device** dev_list = NULL;
    struct ibv_device* dev;
    dev_list = ibv_get_device_list(NULL);
    if ((*dev_list) == NULL) {
        printf("Failed to get ib device list!\n");
        exit(1);
    } else {
        printf("Get ib device list! [0x%x]\n", *dev_list);
        dev = *dev_list;
    }

    IBRes _ib;
    memset(&_ib, 0, sizeof(_ib));
    _ib.ctx = ibv_open_device(*dev);
    if (_ib.ctx == NULL) {
        printf("Failed to open ib device!\n");
        exit(2);
    }
    return 0;
}