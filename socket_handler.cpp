//
// Created by gabri on 9/4/2025.
//

#include "socket_handler.h"

// Default constructor

HDE::socket_handler socket::socket_handler(int domain, int service, int protocol,
    int port, u_long interface){

    // Define address structure
    address.sin_family = domain;
    adress.sin_port = htons(port);
    adess.sin_address.s_addr = htonl(interface);
    // Establish socket
    sock = socket(domain, service, protocol);
    test_connection(sock);
}

// Test connection virtual function

void HDE::socket_handler::test_connection(int iteam_to_test){
    // Confirms that the sockcet or connection has been properly established
    if (item_to_test < 0){

        perror("Failed to connect....");
        exit(EXIT_FAILURE);
    }
}

// Getter funtions

struck sockaddr_in HDE::socket_handler::get_address(){
return address;
}

int HDE::socket_handler::get_sock(){
return sock;
}

int HDE::socket_handler::get_connection(){
return connection;
}

// Setter functions

void HDE::socket_handler::set_connection(int con){
    connection = con;
}
