#include <iostream>
#include <string>
#include <cstdlib>
#include <mysql/mysql.h>
using namespace std;

/*
  What this file does:
  - Connects to our MySQL database (so we can store users and sessions)
  - Lets people register (creates a new user row)
  - Lets people log in (checks email + password)
  - Creates a server-side "session" row so we know who is logged in
  - Puts a "session" cookie in the browser with a random ID (UUID), not the user’s info
  - Lets people log out (deletes the session and clears the cookie)
  - Shows a tiny HTML page if someone opens login.cgi directly

  Important idea:
  *We use server-side sessions.* That means the real "who is logged in" info
  lives in the database, not in the cookie. The cookie only stores a random ID
  that points to the session row. Safer + easier to expire on the server.
*/

// ==== Database connection settings (the info to log into MySQL) ====
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// ==== Numbers we use to describe session status (just labels) ====
// 0 = nobody logged in; 1 = good/active session; 2 = session exists but timed out
const int SESSION_NONE = 0;
const int SESSION_LOGGED_IN = 1;
const int SESSION_EXPIRED = 2;
// This is 5 minutes in seconds. If you’re inactive longer than this, you’re "timed out".
const int SESSION_TIMEOUT = 300;

/*
  htmlEscape(s):
  When we print text into HTML, special characters like < and > can break the page
  or allow "HTML injection". We "escape" those characters so they show up safely.
  This function turns &, <, >, " and ' into safe codes like &amp; and &lt;.
  (We keep this simple and fast for our needs.)
*/
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

/*
  getCookie(name):
  The web server gives us all cookies in an environment variable called HTTP_COOKIE.
  It looks like: "session=abc123; other=xyz".
  We search for "name=" and return the value for that cookie.
  If the cookie doesn't exist, we return an empty string "".
*/
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

/*
  escapeSQL(conn, input):
  This protects us from SQL injection by escaping weird characters before we put
  strings into SQL queries. We MUST do this anytime we mix user input into SQL.
*/
string escapeSQL(MYSQL* conn, const string& input) {
    char* escaped = new char[input.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, input.c_str(), input.length());
    string result = escaped;
    delete[] escaped;
    return result;
}

/*
  urlDecode(str):
  When forms send data, spaces become '+' and special characters become %XX.
  This function converts those back to normal characters so we can read them easily.
*/
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

/*
  getValue(data, key):
  Pulls out a single field from a URL-encoded string like "a=1&b=2".
  It finds "key=" and returns the value. If the key isn’t there, returns "".
  We also call urlDecode so spaces and symbols are correct.
*/
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

/*
  createSession(conn, userId):
  When someone logs in successfully, we make a new row in the sessions table.
  The session_id is made by MySQL using UUID() (which is a big random string).
  We return that session_id so we can put it into a cookie in the user's browser.
*/
string createSession(MYSQL* conn, int userId) {
    string query = "INSERT INTO sessions (session_id, user_id, last_activity) VALUES (UUID(), " + to_string(userId) + ", NOW())";
    
    if (mysql_query(conn, query.c_str())) {
        return "";
    }
    
    // After inserting, grab the newest session for this user (so we know the UUID)
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

/*
  getSessionState(conn, outUserId):
  This checks if the browser has a "session" cookie.
  - If no cookie, then nobody is logged in (SESSION_NONE).
  - If cookie exists, we look it up in the DB:
      * If not found, treat as none (maybe it was deleted).
      * If found, check last_activity time:
          - If more than 5 minutes ago, it’s EXPIRED.
          - Otherwise, it’s LOGGED_IN and we set outUserId to the user’s ID.
*/
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
    
    if (!row) {
        // Cookie said there was a session, but database doesn’t have it ? treat as none
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }
    
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        // Session exists, but it sat too long without activity
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // Still good!
    outUserId = userId;
    return SESSION_LOGGED_IN;
}

/*
  renewSessionActivity(conn, sessionId):
  If the user is active (clicked a page), we update last_activity to NOW().
  This keeps the session alive as long as they keep using the site.
*/
void renewSessionActivity(MYSQL* conn, const string& sessionId) {
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "UPDATE sessions SET last_activity = NOW() WHERE session_id = '" + escapedSid + "'";
    mysql_query(conn, query.c_str());
}

/*
  destroySession(conn, sessionId):
  Deletes the session row from the database. We call this during logout.
*/
void destroySession(MYSQL* conn, const string& sessionId) {
    string escapedSid = escapeSQL(conn, sessionId);
    string query = "DELETE FROM sessions WHERE session_id = '" + escapedSid + "'";
    mysql_query(conn, query.c_str());
}

/*
  clearSessionCookie():
  This tells the browser to forget the "session" cookie by setting it to empty
  and giving it an expiration date in the past (1970). Classic trick.
*/
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {
    /*
      Basic request info:
      - REQUEST_METHOD tells us if the browser did GET or POST.
      - QUERY_STRING holds the stuff after "?" in the URL (like action=logout).
    */
    const char* requestMethod = getenv("REQUEST_METHOD");
    const char* queryString = getenv("QUERY_STRING");
    
    // 1) Connect to the database. If this fails, we can’t do anything else.
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // Minimal error page so the user sees something.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // 2) Figure out the session situation at the very start.
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If logged in and active, keep the session alive by updating last_activity.
    if (sessionState == SESSION_LOGGED_IN) {
        string sessionId = getCookie("session");
        renewSessionActivity(conn, sessionId);
    }
    
    // If the session timed out, clear the browser cookie so the user is "logged out".
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // 3) Handle "logout" action (can come in via ?action=logout).
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
    
    // 4) Handle POST form submissions (either register or login).
    if (requestMethod && string(requestMethod) == "POST") {
        // The browser tells us how many bytes are in the POST body.
        const char* contentLength = getenv("CONTENT_LENGTH");
        int length = contentLength ? atoi(contentLength) : 0;
        
        // Read exactly that many bytes from standard input (cin).
        string postData;
        if (length > 0) {
            char* buffer = new char[length + 1];
            cin.read(buffer, length);
            buffer[length] = '\0';
            postData = buffer;
            delete[] buffer;
        }
        
        // Pull the form fields out of the POST body.
        string action = getValue(postData, "action");     // "register" or "login"
        string email = getValue(postData, "email");
        string password = getValue(postData, "password");
        
        // Escape email and password before using them in SQL (prevents injection).
        string escapedEmail = escapeSQL(conn, email);
        string escapedPassword = escapeSQL(conn, password);
        
        if (action == "register") {
            /*
              Registration:
              We insert a new row into "users" using the email and password they typed.
              (Note: for a real app, we’d hash the password. For this class project,
               we’re keeping it simple and focusing on the overall system.)
            */
            string query = "INSERT INTO users (email, password) VALUES ('" + 
                          escapedEmail + "', '" + escapedPassword + "')";
            
            if (mysql_query(conn, query.c_str())) {
                // Probably a duplicate email. We bounce back with a tiny message.
                cout << "Location: ./index.cgi?error=email_exists\r\n\r\n";
            } else {
                // Success! Ask them to log in now.
                cout << "Location: ./index.cgi?success=registered\r\n\r\n";
            }
        } else if (action == "login") {
            /*
              Login:
              We look up the email + password in the users table.
              If we find a match, we create a new session and set the cookie.
            */
            string query = "SELECT user_id FROM users WHERE email = '" + 
                          escapedEmail + "' AND password = '" + escapedPassword + "'";
            
            if (mysql_query(conn, query.c_str())) {
                // Database problem ? send them back with a basic error flag.
                cout << "Location: ./index.cgi?error=database\r\n\r\n";
            } else {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);
                
                if (row) {
                    // We got a user! Make a session and set the cookie.
                    int loggedInUserId = atoi(row[0]);
                    string sessionId = createSession(conn, loggedInUserId);
                    
                    if (!sessionId.empty()) {
                        // Important: cookie header must come BEFORE Location redirect.
                        cout << "Set-Cookie: session=" << sessionId << "; HttpOnly\r\n";
                        cout << "Location: ./index.cgi\r\n\r\n";
                    } else {
                        // Couldn’t make a session for some reason.
                        cout << "Location: ./index.cgi?error=session\r\n\r\n";
                    }
                } else {
                    // No matching user ? wrong email or password.
                    cout << "Location: ./index.cgi?error=invalid\r\n\r\n";
                }
                
                mysql_free_result(result);
            }
        } else {
            // Some Error send to home
            cout << "Location: ./index.cgi\r\n\r\n";
        }
        // conn = conntions
        mysql_close(conn);
        return 0;
    }
    
    /*
      If someone just opens login.cgi directly in their browser (GET request),
      we show a simple page that explains this file handles login/registration.
      (The "real" form is on index.cgi.)
    */
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
    
    // If the person is already logged in, we show a quick logout form.
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

    // conn = connection
    mysql_close(conn);
    return 0;
}
