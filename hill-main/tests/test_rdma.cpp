#include "rdma/rdma.hpp"
#include "coloring/coloring.hpp"
#include "cmd_parser/cmd_parser.hpp"

#include <infiniband/verbs.h>
#include <iostream>
#include <sstream>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace Hill;
const ColorizedString error_msg("Error: ", Colors::Red);
const ColorizedString warning_msg("Warning: ", Colors::Magenta);

void show_connection_info(const connection_certificate &c, bool is_local = true) {
    std::string from = is_local ? "local" : "remote";
    std::cout << ">> reporting info" << "(" << from << "):\n";
    std::cout << "  [[ address: " << c.addr << "\n";
    std::cout << "  [[ rkey: " << c.rkey << "\n";
    std::cout << "  [[ lid: " << c.lid << "\n";
    std::cout << "  [[ qp num: " << c.qp_num << "\n";
    std::cout << "  [[ gid: ";
    const char *pat = ":%02x";
    for (int i = 0; i < 16; i++) {
        printf(pat + (i == 0), c.gid[i]);
    }
    std::cout << "\n";
}

int socket_connect(bool is_server, int socket_port) {
    struct sockaddr_in seraddr;
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cout << ">> " << error_msg << "can not open socket\n";
        exit(-1);
    }

    memset(&seraddr, 0, sizeof(struct sockaddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(socket_port);

    if (is_server) {
        seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            
        if (bind(sockfd, (struct sockaddr *)&seraddr, sizeof(struct sockaddr)) == -1) {
            std::cout << ">> " << error_msg << "can not bind socket\n";
            exit(-1);
        }

        if (listen(sockfd, 1) == -1) {
            std::cout << ">> " << error_msg << "can not listen socket\n";
            exit(-1);
        }

        auto ret = accept(sockfd, NULL, 0);
        if (ret == -1) {
            std::cout << ">> " << error_msg << "accepting connection failed\n";
            exit(-1);
        }
        return ret;
    } else {
        inet_pton(AF_INET, "127.0.0.1", &seraddr.sin_addr);
        
        if (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) == -1) {
            std::cout << ">> " << error_msg << "connecting to server failed\n";
            exit(-1);
        }
        return sockfd;
    }
}

bool syncop(int sockfd) {
    char msg = 'a';
    if (write(sockfd, &msg, 1) != 1) {
        return false;
    }

    if (read(sockfd, &msg, 1) != 1) {
        return false;
    }
    return true;
}

std::string decode_rdma_status(const RDMAStatus& status) {
    switch(status){
    case Hill::RDMAStatus::Ok:
        return "Ok";
    case Hill::RDMAStatus::NoRDMADeviceList:
        return "NoRDMADeviceList";
    case Hill::RDMAStatus::DeviceNotFound:
        return "DeviceNotFound";
    case Hill::RDMAStatus::NoGID:
        return "NoGID";
    case Hill::RDMAStatus::CannotOpenDevice:
        return "CannotOpenDevice";
    case Hill::RDMAStatus::CannotAllocPD:
        return "CannotAllocPD";
    case Hill::RDMAStatus::CannotCreateCQ:
        return "CannotCreateCQ";
    case Hill::RDMAStatus::CannotRegMR:
        return "CannotRegMR";
    case Hill::RDMAStatus::CannotCreateQP:
        return "CannotCreateQP";
    case Hill::RDMAStatus::CannotQueryPort:
        return "CannotQueryPort";
    case Hill::RDMAStatus::InvalidGIDIdx:
        return "InvalidGIDIdx";
    case Hill::RDMAStatus::InvalidIBPort:
        return "InvalidIBPort";
    case Hill::RDMAStatus::InvalidArguments:
        return "InvalidArguments";
    case Hill::RDMAStatus::CannotInitQP:
        return "CannotInitQP";
    case Hill::RDMAStatus::QPRTRFailed:
        return "QPRTRFailed";
    case Hill::RDMAStatus::QPRTSFailed:
        return "QPRTSFailed";
    case Hill::RDMAStatus::DeviceNotOpened:
        return "DeviceNotOpened";
    case Hill::RDMAStatus::ReadError:
        return "ReadError";
    case Hill::RDMAStatus::WriteError:
        return "WriteError";
    default:
        return "Unknown status";
    }
}

using namespace CmdParser;
int main(int argc, char *argv[]) {
    Parser parser;

    parser.add_option<std::string>("--device", "-d", "mlx5_1");
    parser.add_option<int>("--ib_port", "-p", 1);
    parser.add_option<int>("--socket_port", "-P", 2333);
    parser.add_option<int>("--gid_idx", "-g", 2);
    parser.add_switch("--is_server", "-s", true);
    parser.parse(argc, argv);
    // parser.Parse(argv, argv + argc);
    auto dev_name = parser.get_as<std::string>("--device").value();
    auto ib_port = parser.get_as<int>("--ib_port").value();
    auto socket_port = parser.get_as<int>("--socket_port").value();
    auto gid_idx = parser.get_as<int>("--gid_idx").value();    
    auto is_server = parser.get_as<bool>("--is_server").value();
    
    auto [rdma, status] = RDMA::make_rdma(dev_name, ib_port, gid_idx);
    if (!rdma) {
        std::cerr << "Failed to create RDMA, error code: " << decode_rdma_status(status) << "\n";
        return -1;
    }

    auto buf = new char[1024];
    memset(buf, 0, 1024);
    struct ibv_qp_init_attr at;
    memset(&at, 0, sizeof(struct ibv_qp_init_attr));
    int mr_access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    at.qp_type = IBV_QPT_RC;
    at.sq_sig_all = 1;
    at.cap.max_send_wr = 1;
    at.cap.max_recv_wr = 1;
    at.cap.max_send_sge = 1;
    at.cap.max_recv_sge = 1;

    if (auto status = rdma->open(buf, 1024, 1, mr_access, at); status != RDMAStatus::Ok) {
        std::cerr << "Failed to open RDMA, error code: " << decode_rdma_status(status) << "\n";
        return -1;
    }

    auto sockfd = socket_connect(is_server, socket_port);
    if (rdma->exchange_certificate(sockfd) != RDMAStatus::Ok) {
        std::cerr << "Failed to exchange RDMA, error code: " << decode_rdma_status(status) << "\n";        
    }

    show_connection_info(rdma->get_local());
    show_connection_info(rdma->get_remote(), false);

    auto init_attr = RDMA::get_default_qp_init_attr();
    if (auto [status, err] = rdma->modify_qp(*init_attr, RDMA::get_default_qp_init_attr_mask()); status != RDMAStatus::Ok) {
        std::cerr << "Modify QP to Init failed, error code: " << err << "\n";
        return -1;
    }

    auto rtr_attr = RDMA::get_default_qp_rtr_attr(rdma->get_remote(), rdma->get_ib_port(), rdma->get_gid_idx());
    if (auto [status, err] = rdma->modify_qp(*rtr_attr, RDMA::get_default_qp_rtr_attr_mask()); status != RDMAStatus::Ok) {
        std::cerr << "Modify QP to Rtr failed, error code: " << err << "\n";
        return -1;
    }

    auto rts_attr = RDMA::get_default_qp_rts_attr();
    if (auto [status, err] = rdma->modify_qp(*rts_attr, RDMA::get_default_qp_rts_attr_mask()); status != RDMAStatus::Ok) {
        std::cerr << "Modify QP to Rts failed, error code: " << err << "\n";
        return -1;
    }
    

    syncop(sockfd);
    std::string rdma_msg;

 
    std::cout << ">> buf before send/recv\n";
    for (size_t i = 0; i < rdma_msg.length(); i++) {
        std::cout << buf[i];
    }

    for (int i = 0; i < 10; i++) {
        std::ostringstream stream;
        stream << "Hello RDMA " << i << "\n";
        auto rdma_msg = stream.str();
        if (is_server) {
            sleep(1); // just ensure server posts send AFTER client's recv
            rdma->post_send((uint8_t *)rdma_msg.c_str(), rdma_msg.length());
        } else {
            rdma->post_recv(rdma_msg.length());
        }

        if (rdma->poll_completion() < 0) {
            std::cout << ">> " << error_msg << "polling failed\n";
            return -1;
        }

        std::cout << ">> buf after send/recv\n";
        for (size_t i = 0; i < rdma_msg.length(); i++) {
            std::cout << rdma->get_char_buf()[i];
        }
        std::cout << "\n";
    }

    for (int i = 0; i < 10; i++) {
        std::ostringstream stream;
        stream << "This is a call from client: " << i;
        auto client_msg = stream.str();
        if (!is_server) {
            rdma->post_write((uint8_t *)client_msg.c_str(), client_msg.length());
            rdma->poll_completion();
        }
    
        syncop(sockfd);
        std::cout << ">> buf after rdma write\n";
        for (size_t i = 0; i < client_msg.length(); i++) {
            std::cout << rdma->get_char_buf()[i];
        }
    }

    for (int i = 0; i < 10; i++) {
        std::ostringstream stream;
        stream << "This is a gift from client: " << i;
        auto client_msg = stream.str();
        
        if (!is_server) {
            rdma->fill_buf((uint8_t *)client_msg.c_str(), client_msg.length());
            syncop(sockfd);
        } else {
            syncop(sockfd);
            rdma->post_read(client_msg.length());
            rdma->poll_completion();
        }
    
        std::cout << ">> buf after rdma read\n";
        for (size_t i = 0; i < client_msg.length(); i++) {
            std::cout << rdma->get_char_buf()[i];
        }
    }
    
    close(sockfd);
    return 0;
}
