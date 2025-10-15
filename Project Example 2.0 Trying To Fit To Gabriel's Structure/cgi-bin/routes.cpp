#include "routes.h"
#include "utils.h"

static HttpMethod method_from_env(){ std::string m = getenv_str("REQUEST_METHOD"); if(m=="GET") return HttpMethod::GET; if(m=="POST") return HttpMethod::POST; return HttpMethod::OTHER; }

RouteMatch resolve_route(){
    RouteMatch rm; rm.method = method_from_env();
    std::string pi = get_path_info();
    if(pi.empty()){
        auto qs = parse_form(getenv_str("QUERY_STRING"));
        auto it = qs.find("r"); if(it!=qs.end()) pi = it->second; // e.g., ?r=/auth
    }
    if(pi.empty()) pi = "/";
    rm.path = pi;
    return rm;
}
