//*******************************************
// COMMON SHARED CODE filename: common.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************
// This file contains all shared helper functions and constants
// used across multiple CGI programs in the auction system

#include "common.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <mysql/mysql.h>
using namespace std;

// DATABASE CREDENTIALS:
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// LOGIN STATES:
// we are chosing easy numbers to represent the 3 login states: they are as follows
// 0 = nobody logged in; 1 = active session; 2 = session exists but timed out
const int SESSION_NONE = 0;
const int SESSION_LOGGED_IN = 1;
const int SESSION_EXPIRED = 2;

// INACTIVITY TIMEOUT 
// / logging out the user after 5 minutes (300 seconds) of inactivity
const int SESSION_TIMEOUT = 300;

// PROTECTING AGAINST HTML INJECTION ATTACKS:
// here is an html escape function to handle people trying to inject special characters into the 
// input fields to break into the site. The characters you see are turned into their safe versions
string htmlEscape(const string& s) {
    string r;
    r.reserve(s.size() * 2);
    for (char c : s) {
        if (c == '&') r += "&amp;";
        else if (c == '<') r += "&lt;";
        else if (c == '>') r += "&gt;";
        else if (c == '"') r += "&quot;";
        else if (c == '\'') r += "&#39;";
        else r += c;
    }
    return r;
}

// GET NAME OF COOKIE:
// this function looks for "name=" and returns value for that cookie, or "" if not found
// (browser sends cookies in environment variable called HTTP_COOKIE which is 
// a long string like: "session=abc123; theme=light" )
string getCookie(const string& name) {
    const char* cookies = getenv("HTTP_COOKIE");
    if (!cookies) return "";
    
    string cookieStr = cookies;
    string searchStr = name + "=";
    size_t pos = cookieStr.find(searchStr);
    
    if (pos == string::npos) return "";
    
    pos += searchStr.length();
    size_t endPos = cookieStr.find(";", pos);
    
    if (endPos == string::npos) {
        return cookieStr.substr(pos);
    }
    return cookieStr.substr(pos, endPos - pos);
}

// PROTECT AGAINST SQL INJECTIONS ATTACKS: 
// this is an industry standard function used more than any other to protect
// against SQL injection attacks (so says Google). Before we take user input 
// and send it to MySQL this 'escapes' the weird characters in the string so
// they can't break our database queries
string escapeSQL(MYSQL* conn, const string& input) {
    char* escaped = new char[input.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, input.c_str(), input.length());
    string result = escaped;
    delete[] escaped;
    return result;
}

// CONVERTING WEIRD URL GOBBLEDEYGOOK BACK INTO CHARACTERS:
// this function just decodes url %xx stuff back into characters
string urlDecode(const string& str) {
    string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '+') {
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
            result += static_cast<char>(value);
            i += 2;
        } else {
            result += str[i];
        }
    }
    return result;
}


// SHA-256 HASH
string hashingPass(const string& password){
    unsgned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), passwpord.length(), hash);

    stringsteasm ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// GETTING VALUE PAIRS OUT OF URL GOBBLEDEYGOOK TO SEND TO urlDECODE
// this extracts key=value pairs separated by & to send to urlDecode (above)
// to make sure spaces and symbols are correct. 
// Also, if there is no key it returns an empty string ""
string getValue(const string& data, const string& key) {
    size_t pos = data.find(key + "=");
    if (pos == string::npos) return "";
    
    pos += key.length() + 1;
    size_t endPos = data.find("&", pos);
    
    if (endPos == string::npos) {
        return urlDecode(data.substr(pos));
    }
    return urlDecode(data.substr(pos, endPos - pos));
}

// GETTING SESSION STATE FROM THE SERVER:
// here is where we handle sessions for users logged in or not
// we check for session cookie in browser and if isn't one noone logged in
// if there is cookie we lookup seession ID in database: if not found assume not logged in
// if more than 5 minutes session EXPIRED
// otherwise user is LOGGED_IN and we set outUderId to that user's ID.
int getSessionState(MYSQL* conn, int& outUserId) {
    string sessionId = getCookie("session");
    
    if (sessionId.empty()) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "SELECT user_id, UNIX_TIMESTAMP(last_activity) FROM sessions WHERE session_id = '" + escapedSid + "'";
    
    if (mysql_query(conn, query.c_str())) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    
    // cookie says was a session, but DB doesn't have
    if (!row) {
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }
    
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    // session exists, but user idle too long over 5 mins
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // session good, user active
    outUserId = userId;
    return SESSION_LOGGED_IN;
}

// USER INACTIVITY MONITOR FOR 5 MINUTE AUTOMATIC LOGOUT
// here we update last_activity with NOW() to refresh the 5 minute session
// for users who are still active on the site
void renewSessionActivity(MYSQL* conn, const string& sessionId) {
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "UPDATE sessions SET last_activity = NOW() WHERE session_id = '" + escapedSid + "'";
    mysql_query(conn, query.c_str());
}

// CLEARING THE SESSION COOKIE
// standard highly used way to have the browser forget the session cookie (so says Google)
// by setting it to empty and giving it a 1970 expiration date
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

// FORMATTING TIME REMAINING FOR AUCTION DISPLAY:
// this function calculates how much time is left until an auction ends
// and formats it nicely as "3d 5h 12m" (days, hours, minutes)
// if auction already ended it returns "ENDED"
// NOTE: This function appears in both listings.cpp and transactions.cpp
string formatTimeRemaining(time_t endTime) {
    time_t now = time(0);
    long long secondsLeft = endTime - now;
    
    if (secondsLeft <= 0) {
        return "ENDED";
    }
    
    int days = secondsLeft / 86400;
    int hours = (secondsLeft % 86400) / 3600;
    int minutes = (secondsLeft % 3600) / 60;
    
    string result = "";
    if (days > 0) {
        result += to_string(days) + "d ";
    }
    if (hours > 0 || days > 0) {
        result += to_string(hours) + "h ";
    }
    result += to_string(minutes) + "m";
    
    return result;
}

// HANDLING SESSIONS ON THE SERVER:
// here is where we handle/create the sessions when people log in.
// we make a new row in the database sessions table and use MySQL's UUID feature
// to return a long random string as a session id to use in a cookie in the users browser
// NOTE: This function only appears in login.cpp but included here for completeness
string createSession(MYSQL* conn, int userId) {
    string query = "INSERT INTO sessions (session_id, user_id, last_activity) VALUES (UUID(), " + to_string(userId) + ", NOW())";
    
    if (mysql_query(conn, query.c_str())) {
        return "";
    }
    
    query = "SELECT session_id FROM sessions WHERE user_id = " + to_string(userId) + " ORDER BY created_at DESC LIMIT 1";
    
    if (mysql_query(conn, query.c_str())) {
        return "";
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return "";
    
    MYSQL_ROW row = mysql_fetch_row(result);
    string sessionId = "";
    if (row && row[0]) {
        sessionId = row[0];
    }
    
    mysql_free_result(result);
    return sessionId;
}

// LOGGED-OUT STATE SET BY DELETING THE SESSION ROW FROM THE DATABASE
// this gets called during logout
// NOTE: This function only appears in login.cpp but included here for completeness
void destroySession(MYSQL* conn, const string& sessionId) {
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "DELETE FROM sessions WHERE session_id = '" + escapedSid + "'";
    mysql_query(conn, query.c_str());
}


//Email validation functions

// need testing for this function
bool EmailisValid(const string& email){
    size_t atPos = email.find('@');
    size_t dotPos = email.find('.', atPos);
    return (atPos != string::npos && dotPos != string::npos && atPos > 0 && dotPos > atPos + 1 && dotPos < email.length() - 1);
} 


// need testing for this function
bool isValidEmail(const string& email) {
    const regex pattern(R"((^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$))");
    return regex_match(email, pattern);
} 

