/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-20 15:40:22
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

struct qp_info_t {
    uint32_t lid;
    uint32_t qp_num;
    uint32_t rank;
    uint32_t unused_;
};

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

static void connect()
{
    printf("|connect.\n");
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    ret = getaddrinfo("127.0.0.1", "4396", &hints, &result);

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

    qp_info_t qp_info;
    qp_info.lid = 123;
    qp_info.qp_num = 5;
    qp_info.rank = 1;
    qp_info.sock_write(sock_fd, &qp_info, sizeof(qp_info));
}

int main(int argc, char** argv)
{
    connect();
    return 0;
}