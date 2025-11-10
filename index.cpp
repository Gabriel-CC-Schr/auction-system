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
// user logged out automatically after 5 minutes (=300 seconds)
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

// PROTECTING AGAINST HTML INJECTION ATTACKS:
// here is an html escape function to handle people trying to inject special characters into the 
// input fields to break into the site. The characters you see are turned into their safe versions
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
    
    // cookie says was a session, but DB doesn’t have
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

// CLEARING THE SESSION COOKIE
// standard highly used way to have the browser forget the session cookie (so says Google)
// by setting it to empty and giving it a 1970 expiration date
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {

    // grab query strings from stuff in URL after the "?"
    // need this to show error or success messages
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // 1. connect to database: if fails can’t check sessions or do much else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // We’ll print a basic error page so the user sees something helpful.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // 2. figure out the session state (logged in, expired, or none)
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // if session expired, clear cookie so browser doesn’t think it’s still valid
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // 3. prepare little status message shown at top-left of page
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // 4. start sending our HTML page back to browser
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
    
    // sticky banner in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>All-In Dragons Auctions</h1>\n";
    
    // check URL for ?error= or ?success= to show friendly messages
    string error = getValue(qs, "error");
    string success = getValue(qs, "success");
    
    // if error code, show correct message box
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
            // escape out any weird text before showing it just to be safe
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
    
    // if user already logged in, we don’t show forms.
    // for now we show links to pages that are coming soon
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>You have successfully logged in.</p>\n";
        cout << "      <div class=\"nav\">\n";
        cout << "        <a href=\"#\">Listings (coming soon)</a>\n";
        cout << "        <a href=\"#\">Buy or Sell (coming soon)</a>\n";
        cout << "        <a href=\"#\">My Transactions (coming soon)</a>\n";
        cout << "        <a href=\"login.cgi?action=logout\">Logout</a>\n";
        cout << "      </div>\n";
    } else {
        // login box and forms: where users log in
        cout << "      <h2>Login</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"login\">\n";
        cout << "        <label for=\"login_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"login_email\" name=\"email\" required>\n";
        cout << "        <label for=\"login_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"login_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Login</button>\n";
        cout << "      </form>\n";
        
        // registration box and forms: for users to make new accounts
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
    
    // close up main container and page
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    // closing database at the end 
    mysql_close(conn);
    return 0;
}