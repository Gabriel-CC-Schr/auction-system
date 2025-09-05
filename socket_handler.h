//
// Created by gabri on 9/4/2025.
//

#ifndef AUCTION_SYSTEM_SOCKET_HANDLER_H
#define AUCTION_SYSTEM_SOCKET_HANDLER_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>

namespace HDE{

class socket_handler {
private:
struct sockaddr_in address;
    int sock;
    int connection;
public:
    // Constructor
    socket_handler(int domain, int service, int protocol, int port, u_long interface);
    // Virtual function to connect to a network
    virtual void connect_to_netork(int sock, struct sockaddr_in address) = 0;
    // Function to test sockets and connections
    void test_connection(int);
    // gettter functions
    struct sockaddr_in get_address();
    int get_sock();
    int get_connection();

};
}


#endif //AUCTION_SYSTEM_SOCKET_HANDLER_H