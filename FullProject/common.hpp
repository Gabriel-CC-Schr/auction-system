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
    //   DataBase Config
    static const string DB_HOSt;
    static const string DB_USER;
    static const string DB_PASS;
    static const string DB_NAME;

    // Login states
    static const int SESSION_NONE;
    static const int SESSION_LOGGED_IN;
    static const int SESSION_EXPIRED;

    // Session timeout
    static const int SESSION_TIMEOUT;

    // Public Helpers Functions
    static string htmlEscape(const string& s);
    static string getCookie(const string& name);
    static string escapeSQL(MYSQL* conn, const string& input);
    static string urlDecode(const string& str);
    static string hashingPass(const string& password);
    static string getValue(const string& data, const string& key);
    static int getSessionState(MYSQL* conn, int& outUserId);
    static void renewSessionActivity(MYSQL* conn, const string& sessionId);
    static void clearSessionCookie();
    static string formatTimeRemaining(time_t endTime);
    static string createSession(MYSQL* conn, int userId);
    static void destroySession(MYSQL* conn, const string& sessionId);
    static bool EmailisValid(const string& email);
    static bool isValidEmail(const string& email);
    
    private:
    Common(){}
};

#endif //LOGIN_HTML_COMMON_HPP