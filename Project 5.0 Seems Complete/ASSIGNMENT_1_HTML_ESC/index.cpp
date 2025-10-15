#include <iostream>
#include <string>
#include <cstdlib>
#include <mysql/mysql.h>
using namespace std;

/*
  Hey! This is the main homepage program for our little auction site.
  When someone goes to index.cgi in their browser, the server runs THIS code.

  Big picture of what this file does:
  - Connects to our MySQL database so we can check sessions (who’s logged in).
  - Figures out if the user is logged in, logged out, or timed out.
  - Shows a friendly message at the top-left telling the user their status.
  - If NOT logged in: shows login and register forms.
  - If logged in: shows some “coming soon” links and a logout link.
  - We also read little messages from the URL like ?error=... or ?success=...
    to show quick info boxes to the user.

  Important:
  We use “server-side sessions.” That means the browser only keeps a random
  session ID cookie, and we look up the real info on the server in the database.
  This is safer than putting user info in the cookie itself.
*/

// === These are the details we use to connect to the MySQL database ===
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// === These numbers label what kind of session we found ===
// 0: no one is logged in; 1: logged in and active; 2: session exists but it expired (too much idle time)
const int SESSION_NONE = 0;
const int SESSION_LOGGED_IN = 1;
const int SESSION_EXPIRED = 2;

// === How long a session can sit idle before timing out (5 minutes = 300 seconds) ===
const int SESSION_TIMEOUT = 300;

/*
  htmlEscape(s):
  When we print text onto a web page, special characters like < and & can mess up the HTML
  or let people inject code (which is bad). This function replaces those characters with
  safe versions so the text shows up correctly and safely.
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
  The browser sends cookies to us in an environment variable called HTTP_COOKIE.
  It’s basically a long string like: "session=abc123; theme=light"
  This function looks for "name=" and returns the value for that cookie, or "" if not found.
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
  This helps stop SQL injection by escaping special characters before we put
  the string into a SQL command. SUPER important when handling user input.
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
  When forms send data, spaces become '+' and special characters turn into %XX codes.
  This turns those back into normal characters so we can read them.
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
  This pulls one value out of a query string like "a=1&b=2".
  Example: getValue("error=invalid&foo=bar", "error") returns "invalid".
  We also decode it so + and %XX stuff is handled.
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
  getSessionState(conn, outUserId):
  We check the "session" cookie from the browser. If there isn’t one, then nobody is logged in.
  If there is a cookie, we look up that session ID in the database:
    - If not found ? treat as not logged in.
    - If found, we look at last_activity time:
        * If it’s been more than 5 minutes, the session is EXPIRED.
        * Otherwise, it’s LOGGED_IN, and we set outUserId to that user’s ID.
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
        // The cookie said there was a session, but the DB doesn’t have it (maybe it was deleted)
        mysql_free_result(result);
        outUserId = 0;
        return SESSION_NONE;
    }
    
    int userId = atoi(row[0]);
    long lastActivity = atol(row[1]);
    mysql_free_result(result);
    
    long currentTime = time(NULL);
    if (currentTime - lastActivity > SESSION_TIMEOUT) {
        // The session exists, but the user was idle too long (over 5 mins)
        outUserId = 0;
        return SESSION_EXPIRED;
    }
    
    // Good session! User is active.
    outUserId = userId;
    return SESSION_LOGGED_IN;
}

/*
  clearSessionCookie():
  This tells the browser to delete the "session" cookie by setting it to empty
  and giving it an old expiration date (from the past).
*/
void clearSessionCookie() {
    cout << "Set-Cookie: session=; HttpOnly; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
}

int main() {
    /*
      Grab the query string from the URL (the part after the "?").
      Example: if the URL is index.cgi?error=invalid, then QUERY_STRING is "error=invalid".
      We’ll use this to show error/success messages if needed.
    */
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // Step 1: Connect to the database. If this fails, we can’t check sessions or do much else.
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // We’ll print a basic error page so the user sees something helpful.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // Step 2: Figure out the session situation right away (logged in, expired, or none).
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If the session expired, we clear the cookie so the browser doesn’t think it’s still valid.
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // Step 3: Prepare the little status message we show at the top-left of the page.
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // Step 4: Start sending our HTML page back to the browser.
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
    
    // The sticky banner in the top-left that tells the user their login status.
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // Main content container (just a nice white box with stuff in it).
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>All-In Dragons Auctions</h1>\n";
    
    // We check the URL for ?error=... or ?success=... so we can show friendly messages.
    string error = getValue(qs, "error");
    string success = getValue(qs, "success");
    
    // If there was an error code, show the right message box.
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
            // If it’s some random text, escape it before showing (to be safe).
            cout << "        " << htmlEscape(error) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // If there was a success message, show that too.
    if (!success.empty()) {
        cout << "      <div class=\"success\">\n";
        if (success == "registered") {
            cout << "        Registration successful! Please login below.\n";
        } else {
            cout << "        " << htmlEscape(success) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // If the user is already logged in, we don’t show the forms.
    // Instead, we show some navigation links (they’re placeholders for now).
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>Welcome back! You are successfully logged in.</p>\n";
        cout << "      <div class=\"nav\">\n";
        cout << "        <a href=\"#\">Listings (coming soon)</a>\n";
        cout << "        <a href=\"#\">Buy or Sell (coming soon)</a>\n";
        cout << "        <a href=\"#\">My Transactions (coming soon)</a>\n";
        cout << "        <a href=\"login.cgi?action=logout\">Logout</a>\n";
        cout << "      </div>\n";
    } else {
        // Not logged in yet? Then show the Login form first…
        cout << "      <h2>Login</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"login\">\n";
        cout << "        <label for=\"login_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"login_email\" name=\"email\" required>\n";
        cout << "        <label for=\"login_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"login_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Login</button>\n";
        cout << "      </form>\n";
        
        // …and then the Register form so new people can make an account.
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
    
    // Close up the main container and the page.
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    // Always close the database connection when we’re done.
    mysql_close(conn);
    return 0;
}
