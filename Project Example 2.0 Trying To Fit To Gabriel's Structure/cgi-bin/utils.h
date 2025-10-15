#pragma once
#include <string>
#include <map>

// Basic CGI/HTTP helpers
void http_headers(const std::string& ctype = "application/json", const std::string& extra = "");
void http_headers_with_status(int code, const std::string& reason, const std::string& ctype = "application/json", const std::string& extra = "");

// Body / query / form parsing
std::string read_body();
std::string url_decode(const std::string& s);
std::map<std::string,std::string> parse_form(const std::string& x_www_form_urlencoded);

// JSON escaping
std::string json_escape(const std::string& s);

// Env helpers
std::string getenv_str(const char* name);
std::string get_path_info();
std::map<std::string,std::string> parse_cookies(const std::string& cookie_header);
std::string cookie_get(const std::map<std::string,std::string>& jar, const std::string& key);

// Random hex
std::string rand_hex(size_t nbytes);
