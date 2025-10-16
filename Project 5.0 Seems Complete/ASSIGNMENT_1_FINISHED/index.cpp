//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: index.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

#include <iostream>
#include <string>
#include <cstdlib>
#include <mysql/mysql.h>
using namespace std;

// The DB CREDENTIALS
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// CREATING SESSION STATES AND TIMEOUT DURATION
const int SESSION_NONE = 0;
const int SESSION_LOGGED_IN = 1;
const int SESSION_EXPIRED = 2;

const int SESSION_TIMEOUT = 300;// 5 minutes for inactivity

// here is an html escape function to handle people trying to inject special characters
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

// this function reads a cookie from the browser and returns its value
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

// ESCAPING STRINGS FOR SAFE USE IN SQL QUERIES:
string escapeSQL(MYSQL* conn, const string& input) {
    char* escaped = new char[input.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, input.c_str(), input.length());
    string result = escaped;
    delete[] escaped;
    return result;
}

// this function decodes URL-encoded strings (like form data)
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

// this function pulls a value out of a URL query string or POST body
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

// HANDLING SESSIONS ON THE SERVER:
// this function checks the session cookie and returns the session state
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
    
    // session cookie exists but no matching session in database
    if (!row) {
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }
    
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    // check for session timeout due to inactivity
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // session is valid and active
    outUserId = userId;
    return SESSION_LOGGED_IN;
}

// this function clears the session cookie in the browser
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {

    // read the QUERY_STRING environment variable for any URL parameters
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // connect to the database
    MYSQL* conn = mysql_init(NULL);
    // if connection fails, show basic error page
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If user is logged in renew session activity to prevent timeout.
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // if user is logged in renew session activity to prevent timeout
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // show the main page
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>All-In Dragons Auctions - Home</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";

    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n"; // show status at top left
    
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>All-In Dragons Auctions</h1>\n";
    
    // show any error or success messages from URL parameters
    string error = getValue(qs, "error");
    string success = getValue(qs, "success");
    
    // show error message
    if (!error.empty()) {
        cout << "      <div class=\"error\">\n";
        if (error == "email_exists") {
            cout << "        Email already registered. Please login or use a different email.\n";
        } else if (error == "invalid") {
            cout << "        Invalid email or password. Please try again.\n";
        } else if (error == "database") {
            cout << "        Database error. Please try again later.\n";
        } else if (error == "session") {
            cout << "        Session creation failed. Please try again.\n";
        } else {
            
            cout << "        " << htmlEscape(error) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // show success message
    if (!success.empty()) {
        cout << "      <div class=\"success\">\n";
        if (success == "registered") {
            cout << "        Registration successful! Please login below.\n";
        } else {
            cout << "        " << htmlEscape(success) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // if logged in show user nav links, else show login and registration forms
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>You have successfully logged in.</p>\n";
        cout << "      <div class=\"nav\">\n";
        cout << "        <a href=\"#\">Listings (coming soon)</a>\n";
        cout << "        <a href=\"#\">Buy or Sell (coming soon)</a>\n";
        cout << "        <a href=\"#\">My Transactions (coming soon)</a>\n";
        cout << "        <a href=\"login.cgi?action=logout\">Logout</a>\n";
        cout << "      </div>\n";
    } else { // not logged in so show login and registration forms
        cout << "      <h2>Login</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"login\">\n";
        cout << "        <label for=\"login_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"login_email\" name=\"email\" required>\n";
        cout << "        <label for=\"login_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"login_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Login</button>\n";
        cout << "      </form>\n";
        
        // registration form
        cout << "      <h2>Register New Account</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"register\">\n";
        cout << "        <label for=\"register_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"register_email\" name=\"email\" required>\n";
        cout << "        <label for=\"register_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"register_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Register</button>\n";
        cout << "      </form>\n";
    }
    
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
 
    mysql_close(conn);
    return 0;
}
