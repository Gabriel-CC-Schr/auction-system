//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: login.cpp
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
// after 5 minutes of inactivity (300 seconds) users are logged out
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

// HANDLING COOKIES: here is where we check to see if cookies exist (getCookie), 
// and if no cookie exists we return an empty string ""
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

// HANDLING SESSIONS ON THE SERVER:
// here is where we handle/create the sessions when people log in.
// we make a new row in the database sessions table and use MySQL's UUID feature
// to return a long random string as a session id to use in a cookie in the users browser
string createSession(MYSQL* conn, int userId) {
    string query = "INSERT INTO sessions (session_id, user_id, last_activity) VALUES (UUID(), " + to_string(userId) + ", NOW())";
    
    if (mysql_query(conn, query.c_str())) {
        return "";
    }
    
    // After insert grab the newest session for this user so we know the UUID
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

// CHECKING USERS LOGIN STATE:
// This function figures out whether the user is currently logged in, 
// logged out, or has a session that timed out.
int getSessionState(MYSQL* conn, int& outUserId) {
    // tries to read a cookie named session from the browser.
    string sessionId = getCookie("session");
    
    // if no cookie, user isn’t logged in
    if (sessionId.empty()) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    // escapes any special characters to prevent SQL injection (escapeSQL)
    string escapedSid = escapeSQL(conn, sessionId);
    // make an SQL query to look for session ID in sessions table
    string query = "SELECT user_id, UNIX_TIMESTAMP(last_activity) FROM sessions WHERE session_id = '" + escapedSid + "'";
    
    // runs the SQL command and if anything weird user is logged out
    if (mysql_query(conn, query.c_str())) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    // grabs query result from SQL, but assumes user logged out if it cant
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        outUserId = 0;
        return SESSION_NONE;
    }
    
    // fetches first and only row from query result and if no match then cookie is useless
    // so outUserId is set to 0, it returns SESSION_HOME and frees up memory for results
    MYSQL_ROW row = mysql_fetch_row(result);
    
    // cookie says there's a session but database doesn’t, so there is none
    if (!row) {
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }

    // converts database strings into numbers:
    // userId: numeric ID of logged in user.
    // lastActivity: in seconds since 1970 UNIX time.
    // frees MySQL result memory
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    // too long without activity user booted
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // if we get all the way down here the session still good
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

// LOGGED-OUT STATE SET BY DELETING THE SESSION ROW FROM THE DATABASE
// this gets called during logout
void destroySession(MYSQL* conn, const string& sessionId) {
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "DELETE FROM sessions WHERE session_id = '" + escapedSid + "'";
    mysql_query(conn, query.c_str());
}

// CLEARING THE SESSION COOKIE
// standard highly used way to have the browser forget the session cookie (so says Google)
// by setting it to empty and giving it a 1970 expiration date
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {

    // INFO REQUESTS
    // REQUEST_METHOD tells us if browser did GET or POST
    // QUERY_STRING holds stuff after "?" in the URL (like action=logout)
    const char* requestMethod = getenv("REQUEST_METHOD");
    const char* queryString = getenv("QUERY_STRING");
    
    // 1. connect to the database. If fails, can’t do anything else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // Minimal error page so the user sees something.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // 2. figure out session situation at the very start
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If logged in and active keep session alive by updating last_activity.
    if (sessionState == SESSION_LOGGED_IN) {
        string sessionId = getCookie("session");
        renewSessionActivity(conn, sessionId);
    }
    
    // If session timed out clear browser cookie so user is "logged out".
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // 3. handle logout action by ?action=logout
    if (queryString && string(queryString).find("action=logout") != string::npos) {
        string sessionId = getCookie("session");
        if (!sessionId.empty()) {
            destroySession(conn, sessionId);   // remove from DB
        }
        clearSessionCookie();                   // remove from browser
        cout << "Location: ./index.cgi\r\n\r\n"; // send them back home
        mysql_close(conn);
        return 0;
    }
    
    // 4. handle POST form submissions either register or login
    if (requestMethod && string(requestMethod) == "POST") {
        // The browser tells us how many bytes are in the POST body.
        const char* contentLength = getenv("CONTENT_LENGTH");
        int length = contentLength ? atoi(contentLength) : 0;
        
        // read exactly that many bytes from cin
        string postData;
        if (length > 0) {
            char* buffer = new char[length + 1];
            cin.read(buffer, length);
            buffer[length] = '\0';
            postData = buffer;
            delete[] buffer;
        }
        
        // pull form fields out of the POST body.
        string action = getValue(postData, "action");     // "register" or "login"
        string email = getValue(postData, "email");
        string password = getValue(postData, "password");
        
        // sql injection prevention:
        // escape email and password before using in SQL 
        string escapedEmail = escapeSQL(conn, email);
        string escapedPassword = escapeSQL(conn, password);
        
        // REGISTERING NEW USERS:
        // insert a new row into "users" using email and password typed
        if (action == "register") {
            string query = "INSERT INTO users (email, password) VALUES ('" + 
                          escapedEmail + "', '" + escapedPassword + "')";
            
            if (mysql_query(conn, query.c_str())) {
                // duplicate email send message to user
                cout << "Location: ./index.cgi?error=email_exists\r\n\r\n";
            } else {
                // registered successfully, tell user to log in
                cout << "Location: ./index.cgi?success=registered\r\n\r\n";
            }
        } else if (action == "login") {
            // look up email and password in users table and upon a match
            // create a new session and set cookie          
            string query = "SELECT user_id FROM users WHERE email = '" + 
                          escapedEmail + "' AND password = '" + escapedPassword + "'";
            
            // database problems: send user a basic error
            if (mysql_query(conn, query.c_str())) {
                cout << "Location: ./index.cgi?error=database\r\n\r\n";
            } else {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);
                
                // we got a user! make session set cookie
                if (row) {
                    int loggedInUserId = atoi(row[0]);
                    string sessionId = createSession(conn, loggedInUserId);

                    
                    if (!sessionId.empty()) {
                        // IMPORTANT: cookie header must come BEFORE Location redirect
                        cout << "Set-Cookie: session=" << sessionId << "; HttpOnly\r\n";
                        cout << "Location: ./index.cgi\r\n\r\n";
                    } else {
                        // couldn’t make session for some reason send user a basic error
                        cout << "Location: ./index.cgi?error=session\r\n\r\n";
                    }
                } else {
                    // no matching user send wrong email or password message
                    cout << "Location: ./index.cgi?error=invalid\r\n\r\n";
                }
                
                mysql_free_result(result);
            }
        } else {
            // if anything else happens (weird) send user home
            cout << "Location: ./index.cgi\r\n\r\n";
        }
        
        mysql_close(conn);
        return 0;
    }
    
    // DIRECTLY ACCESSING LOGIN.CGI:
    // this basic page shows up when directly accessing login.cgi
    // whereas the actual form is on index.cgi
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>Authentication Handler</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>Authentication Handler</h1>\n";
    cout << "      <p>This page handles login and registration.</p>\n";
    
    // if the user already logged in we show a quick logout form.
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>You are currently logged in.</p>\n";
        cout << "      <form action=\"login.cgi\" method=\"get\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"logout\">\n";
        cout << "        <button type=\"submit\" class=\"logout-btn\">Logout</button>\n";
        cout << "      </form>\n";
    }
    
    cout << "      <div class=\"nav\">\n";
    cout << "        <a href=\"index.cgi\">Return to Home</a>\n";
    cout << "      </div>\n";
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    // closing database at the end 
    mysql_close(conn);
    return 0;
}
