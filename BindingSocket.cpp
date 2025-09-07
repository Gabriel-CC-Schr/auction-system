//
// Created by gabri on 9/7/2025.
//

#include "BindingSocket.h"

HDE::BindingSocket::BindingSOcket)(int domain,int service, 
int protocol, int port, u_long interface){

    set_connection(connect_to_network(get_sock(), get_address()));
    test_connection(get_connection());

}

int HDE::BindingSocket::connect_to_network(int sock, struct sockaddr_in address){

    return bind(sock,(struct sockaddr*)&address, sizeof(address));
}