#include <iostream>
#include <string>
#include <cstdlib>
#include <mysql/mysql.h>
using namespace std;

// Database connection constants
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// Session state constants: 0=NONE, 1=LOGGED_IN, 2=EXPIRED
const int SESSION_NONE = 0;
const int SESSION_LOGGED_IN = 1;
const int SESSION_EXPIRED = 2;

// Session timeout: 5 minutes = 300 seconds
const int SESSION_TIMEOUT = 300;

// Helper function: Get value of a cookie by name
// Parses HTTP_COOKIE environment variable
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

// Helper function: Escape SQL special characters to prevent injection
string escapeSQL(MYSQL* conn, const string& input) {
    char* escaped = new char[input.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, input.c_str(), input.length());
    string result = escaped;
    delete[] escaped;
    return result;
}

// Helper function: URL decode a string (handles %20, +, etc.)
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

// Helper function: Get query parameter value by name
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

// Session helper: Check session state and get userId
// Returns SESSION_NONE, SESSION_LOGGED_IN, or SESSION_EXPIRED
// Sets outUserId if session is valid (LOGGED_IN)
int getSessionState(MYSQL* conn, int& outUserId) {
    string sessionId = getCookie("session");
    
    // No cookie means never logged in
    if (sessionId.empty()) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    // Look up session in database
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
    
    if (!row) {
        // Session ID not found in database
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }
    
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    // Check if session expired (more than 5 minutes since last activity)
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // Session is valid
    outUserId = userId;
    return SESSION_LOGGED_IN;
}

// Session helper: Clear session cookie (set expired date)
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {
    // Get query string for error/success messages
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // Initialize MySQL connection
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // Check session state early
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If expired, clear the cookie
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // Determine status message based on session state
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // Send HTML response
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>Auction Site - Home</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    
    // Status banner at top-left
    cout << "    <div class=\"status\"><em>" << statusMessage << "</em></div>\n";
    
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>Welcome to the Auction Site</h1>\n";
    
    // Display error or success messages from query string
    string error = getValue(qs, "error");
    string success = getValue(qs, "success");
    
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
        }
        cout << "      </div>\n";
    }
    
    if (!success.empty()) {
        cout << "      <div class=\"success\">\n";
        if (success == "registered") {
            cout << "        Registration successful! Please login below.\n";
        }
        cout << "      </div>\n";
    }
    
    // Show different content based on login state
    if (sessionState == SESSION_LOGGED_IN) {
        // User is logged in - show logged-in content
        cout << "      <p>Welcome back! You are successfully logged in.</p>\n";
        cout << "      <div class=\"nav\">\n";
        cout << "        <a href=\"listings.cgi\">View Auctions</a>\n";
        cout << "        <a href=\"trade.cgi\">Create Auction / Place Bid</a>\n";
        cout << "        <a href=\"transactions.cgi\">My Transactions</a>\n";
        cout << "        <a href=\"login.cgi?action=logout\">Logout</a>\n";
        cout << "      </div>\n";
    } else {
        // User is not logged in - show login and registration forms
        cout << "      <h2>Login</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"login\">\n";
        cout << "        <label for=\"login_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"login_email\" name=\"email\" required>\n";
        cout << "        <label for=\"login_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"login_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Login</button>\n";
        cout << "      </form>\n";
        
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