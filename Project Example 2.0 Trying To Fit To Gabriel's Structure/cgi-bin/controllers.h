#pragma once
#include <string>
#include <map>
#include <mariadb/mysql.h>

enum class HttpMethod { GET, POST, OTHER };

void handle_auth(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body);
void handle_auctions(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body);
void handle_transactions(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body);
