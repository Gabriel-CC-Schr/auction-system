#include "utils.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

void http_headers(const std::string& ctype, const std::string& extra){
    std::cout << "Content-Type: " << ctype << "\r\n";
    if(!extra.empty()) std::cout << extra;
    std::cout << "\r\n";
}

void http_headers_with_status(int code, const std::string& reason, const std::string& ctype, const std::string& extra){
    std::cout << "Status: " << code << " " << reason << "\r\n";
    std::cout << "Content-Type: " << ctype << "\r\n";
    if(!extra.empty()) std::cout << extra;
    std::cout << "\r\n";
}

std::string read_body(){
    const char* cl = std::getenv("CONTENT_LENGTH"); size_t n = cl? std::strtoul(cl,nullptr,10) : 0;
    std::string body; body.resize(n);
    if(n) fread(body.data(),1,n,stdin);
    return body;
}

std::string url_decode(const std::string& s){
    std::string o; o.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        if(s[i]=='+') o.push_back(' ');
        else if(s[i]=='%' && i+2<s.size()){
            int v=0; std::sscanf(s.substr(i+1,2).c_str(), "%x", &v); o.push_back(char(v)); i+=2;
        } else o.push_back(s[i]);
    }
    return o;
}

std::map<std::string,std::string> parse_form(const std::string& s){
    std::map<std::string,std::string> m; size_t i=0;
    while(i<s.size()){
        size_t e=s.find('=',i); if(e==std::string::npos) break;
        size_t a=s.find('&',e+1);
        std::string k=url_decode(s.substr(i,e-i));
        std::string v=url_decode(s.substr(e+1,(a==std::string::npos?s.size():a)-(e+1)));
        m[k]=v; if(a==std::string::npos) break; i=a+1;
    }
    return m;
}

std::string json_escape(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for(unsigned char c: s){
        switch(c){
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\b': o += "\\b"; break;
            case '\f': o += "\\f"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if(c<0x20){ char buf[7]; std::sprintf(buf,"\\u%04x", c); o+=buf; }
                else o.push_back(c);
        }
    }
    return o;
}

std::string getenv_str(const char* name){ const char* v=std::getenv(name); return v? std::string(v):std::string(); }
std::string get_path_info(){ return getenv_str("PATH_INFO"); }

std::map<std::string,std::string> parse_cookies(const std::string& cookie_header){
    std::map<std::string,std::string> m; if(cookie_header.empty()) return m;
    size_t i=0; while(i<cookie_header.size()){
        size_t sc = cookie_header.find(';', i);
        std::string part = cookie_header.substr(i, (sc==std::string::npos? cookie_header.size():sc)-i);
        size_t eq = part.find('=');
        if(eq!=std::string::npos){
            std::string k = part.substr(0,eq); std::string v = part.substr(eq+1);
            // trim spaces
            while(!k.empty() && (k.front()==' '||k.front()=='\t')) k.erase(k.begin());
            while(!v.empty() && (v.front()==' '||v.front()=='\t')) v.erase(v.begin());
            m[k]=v;
        }
        if(sc==std::string::npos) break; i=sc+1;
    }
    return m;
}

std::string cookie_get(const std::map<std::string,std::string>& jar, const std::string& key){
    auto it = jar.find(key); return it==jar.end()? std::string() : it->second;
}

std::string rand_hex(size_t nbytes){
    static std::random_device rd; static std::mt19937_64 gen(rd()); std::uniform_int_distribution<int> d(0,255);
    std::string s; s.reserve(nbytes*2); char buf[3];
    for(size_t i=0;i<nbytes;i++){ std::sprintf(buf, "%02x", d(gen)); s+=buf; }
    return s;
}
