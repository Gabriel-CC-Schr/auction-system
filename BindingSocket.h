//
// Created by gabri on 9/7/2025.
//

#ifndef AUCTION_SYSTEM_BINDINGSOCKET_H
#define AUCTION_SYSTEM_BINDINGSOCKET_H

#include <stdio.h>

#include <"socket_handler.h">


namespace HDE {

class BindingSocket:: public socket_handler {
public:
    BindingSocket(int domain, int service, int protocol, int port, u_long interface)
    : socket_handler(domain, service, protocol, port, interface) {};

    int connect_to_network(int sock, struck sockaddr_in address)
}
}




#endif //AUCTION_SYSTEM_BINDINGSOCKET_H