// Single CGI entrypoint that routes to controllers
#include "utils.h"
#include "routes.h"
#include "db.h"
#include <iostream>

int main(){
    RouteMatch rm = resolve_route();
    auto qs = parse_form(getenv_str("QUERY_STRING"));
    std::string body = (rm.method==HttpMethod::POST)? read_body(): std::string();

    MYSQL* c = db_connect();
    if(!c){ http_headers(); std::cout << "{\"ok\":false,\"error\":\"db\"}"; return 0; }

    if(rm.path=="/auth"){
        handle_auth(c, rm.method, qs, body);
    } else if(rm.path=="/auctions"){
        handle_auctions(c, rm.method, qs, body);
    } else if(rm.path=="/transactions"){
        handle_transactions(c, rm.method, qs, body);
    } else {
        http_headers(); std::cout << "{\"ok\":false,\"error\":\"route\"}";
    }

    mysql_close(c);
    return 0;
}
