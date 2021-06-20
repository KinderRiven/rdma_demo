/*
 * @Author: your name
 * @Date: 2021-06-17 10:56:52
 * @LastEditTime: 2021-06-20 15:20:05
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
}

int main(int argc, char** argv)
{
    connect();
    return 0;
}