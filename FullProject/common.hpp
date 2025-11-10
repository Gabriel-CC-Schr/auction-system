//
// Created by gabri on 11/10/2025.
//

#ifndef LOGIN_HTML_COMMON_HPP
#define LOGIN_HTML_COMMON_HPP
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <mysql/mysql.h>

using namespace std;


class common {
public:
    std::string htmlEscape(const std::string& s);
};

#endif //LOGIN_HTML_COMMON_HPP