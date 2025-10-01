//
// Created by gabri on 9/4/2025.
//

#include <iostream>
#include "mongoose.h"
#include <string>

using namespace std;

// Struct containing settings for how to server htth with mongoose
stactic struct mg_server_http_opst s_http_server_opts;

// Event handler
static void ev_hanfler(struct mg_connection *nc, int ev, void *p){
    //if event is a htpp request
    if(ev == MG_EV_HTTP_REQUEST){
        // Server static html files
        mg_server_htto(nc, (stuct http_message *) p, s_http_server_opts);

}
}

int initServer(int port){

    // Mongoose event manager
    struct mg_mgr mgr;
    // Mongoose connection
    struct mg_connection *nc;

    // Convert port to char
    std::string portToChar = std::to_string(port);
    static char const *sPort = portToChar.c_str(); // *sPort to set the port

    //Init mongoose
    mg_mgr_init(&mgr, NULL);
    cout << "Starting Web server on port " << sPort << endl;

    nc = mg_bind(&mgr, sPort, ev_handler);

    // If the connection fails
    if (nc == NULL) {
        cout << "Failed to create listener" << endl;
        return -1;
    }

    // Set op Http server options
    mg_set_protocol_http_websocket(nc);

    s_http_server_opts.document_root = ".";

    s_http_server_opts.enable_directory_listing = "yes";

    for(;;){
        mg_mgr_poll(&mgr, 1000);
    }

    //Free up all memory allocate
    mg_mgr_free(&mgr);
    return 0;

}



int main(void){
    int port;
    cout << "Select server port" << endl;

    cin >> port;

    //Fail case
    if(cin.fail()){
    port = 1000;
    }

    initServer(port);

    return 0;

}