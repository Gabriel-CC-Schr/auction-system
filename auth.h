//
// Created by gabri on 9/4/2025.
//
#ifndef AUCTION_SYSTEM_AUTH_H
#define AUCTION_SYSTEM_AUTH_H

#include <string>
#include <mysql/mysql.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

class auth {
// Public methods for authentication
public:

    // URL decode function
    static std::string urlDecode(const std::string& str);
    // Get POST value function
    static std::string getPostValue(const std::string& postData, const std::string& fieldName);
    // Escape SQL function
    static std::string escapeSQL(MYSQL* conn, const std::string& str);
    // Get cookie value function
    static std::string getCookie(const std::string& cookieName);
    // Check session function
    static int checkSession();
    // This is a custom SQL escape function
    static char* spc_escape_sql(const char *input, char quote, int wildcards);

private:
    // Set session cookie function
    static void setSessionCookie(int userId);

    static int getUserIdFromSession();
    // Add other private members or methods if needed

};


#endif