//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: listings.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

// listings.cpp - Display all auction listings
// This file shows all open auctions, ordered by ending soonest

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() and atoi()

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

// FORMATTING TIME REMAINING FOR AUCTION DISPLAY:
// this function calculates how much time is left until an auction ends
// and formats it nicely as "3d 5h 12m" (days, hours, minutes)
// if auction already ended it returns "ENDED"
string formatTimeRemaining(time_t endTime) {
    time_t now = time(0);                // get current time
    long long secondsLeft = endTime - now;  // calculate seconds remaining
    
    if (secondsLeft <= 0) {              // if auction has ended
        return "ENDED";
    }
    
    // Calculate days, hours, minutes
    int days = secondsLeft / 86400;      // 86400 seconds in a day
    int hours = (secondsLeft % 86400) / 3600;  // 3600 seconds in an hour
    int minutes = (secondsLeft % 3600) / 60;   // 60 seconds in a minute
    
    // Build formatted string
    string result = "";
    if (days > 0) {                      // if more than a day left
        result += to_string(days) + "d ";
    }
    if (hours > 0 || days > 0) {         // if hours or days
        result += to_string(hours) + "h ";
    }
    result += to_string(minutes) + "m";  // always show minutes
    
    return result;
}

int main() {

    // 1. CONNECT TO DATABASE FIRST:
    // we need database connection to check sessions and query auction listings
    // if connection fails we show a basic error page
    MYSQL* conn = mysql_init(NULL);
    if (conn == NULL) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "  <head>\n";
        cout << "    <meta charset=\"UTF-8\">\n";
        cout << "    <title>Database Error</title>\n";
        cout << "  </head>\n";
        cout << "  <body>\n";
        cout << "    <h1>Database Error</h1>\n";
        cout << "    <p>Failed to initialize database connection.</p>\n";
        cout << "  </body>\n";
        cout << "</html>\n";
        return 1;
    }
    
    // actually connect to the database using our credentials
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "  <head>\n";
        cout << "    <meta charset=\"UTF-8\">\n";
        cout << "    <title>Database Error</title>\n";
        cout << "  </head>\n";
        cout << "  <body>\n";
        cout << "    <h1>Database Error</h1>\n";
        cout << "    <p>Failed to connect to database: " << htmlEscape(mysql_error(conn)) << "</p>\n";
        cout << "  </body>\n";
        cout << "</html>\n";
        mysql_close(conn);
        return 1;
    }
    
    // 2. CHECK SESSION STATE:
    // listings page doesn't require login but changes display if user is logged in
    // logged in users see bid buttons, logged out users see login link
    int currentUserId = 0;
    int sessionState = getSessionState(conn, currentUserId);
    
    // if logged in, renew activity timestamp to keep session alive
    if (sessionState == SESSION_LOGGED_IN) {
        string sessionId = getCookie("session");
        renewSessionActivity(conn, sessionId);
    }
    
    // 3. PREPARE STATUS MESSAGE FOR BANNER:
    // this little message shows at top-left of page telling user their login status
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // 4. UPDATE EXPIRED AUCTIONS:
    // before showing listings, mark any auctions that passed their end time as closed
    // this keeps the database clean and ensures we only show active auctions
    string updateQuery = "UPDATE auctions SET is_closed = TRUE WHERE end_time < NOW() AND is_closed = FALSE";
    mysql_query(conn, updateQuery.c_str());
    
    // 5. QUERY FOR ALL OPEN AUCTIONS:
    // get all auctions that aren't closed yet, ordered by end time (soonest first)
    // this way users see auctions that are ending soon at the top of the list
    string listQuery = "SELECT auction_id, seller_id, item_description, starting_bid, "
                      "current_bid, start_time, end_time, winner_id "
                      "FROM auctions WHERE is_closed = FALSE "
                      "ORDER BY end_time ASC";
    
    if (mysql_query(conn, listQuery.c_str())) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "  <head>\n";
        cout << "    <meta charset=\"UTF-8\">\n";
        cout << "    <title>Database Error</title>\n";
        cout << "  </head>\n";
        cout << "  <body>\n";
        cout << "    <h1>Database Error</h1>\n";
        cout << "    <p>Failed to fetch auctions: " << htmlEscape(mysql_error(conn)) << "</p>\n";
        cout << "  </body>\n";
        cout << "</html>\n";
        mysql_close(conn);
        return 1;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    
    // 6. START BUILDING HTML PAGE:
    // we send HTTP header first then start building the page structure
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>Auction Listings</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    
    // sticky banner in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>Current Auctions</h1>\n";
    
    // NAVIGATION MENU:
    // links change based on whether user is logged in or not
    cout << "      <div class=\"nav\">\n";
    cout << "        <a href=\"./listings.cgi\">Listings</a>\n";
    
    if (currentUserId > 0) {
        // logged in: show auction management links
        cout << "        <a href=\"./trade.cgi\">Buy/Sell</a>\n";
        cout << "        <a href=\"./transactions.cgi\">My Transactions</a>\n";
        cout << "        <a href=\"./login.cgi?action=logout\">Logout</a>\n";
    } else {
        // not logged in: show login link
        cout << "        <a href=\"./index.cgi\">Login/Register</a>\n";
    }
    
    cout << "      </div>\n";
    
    // 7. DISPLAY AUCTION LISTINGS:
    // check if there are any auctions to show
    if (mysql_num_rows(result) == 0) {
        cout << "      <p>No active auctions at this time.</p>\n";
    } else {
        // display auctions in a table format
        cout << "      <table>\n";
        cout << "        <tr>\n";
        cout << "          <th>Item Description</th>\n";
        cout << "          <th>Current Bid</th>\n";
        cout << "          <th>Time Remaining</th>\n";
        
        // only show action column if user is logged in
        if (currentUserId > 0) {
            cout << "          <th>Action</th>\n";
        }
        cout << "        </tr>\n";
        
        // loop through each auction result and display it
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            int auctionId = atoi(row[0]);
            int sellerId = atoi(row[1]);
            string itemDesc = row[2];
            string startingBid = row[3];
            string currentBid = row[4];
            string startTime = row[5];
            string endTimeStr = row[6];
            
            // convert end time string to time_t so we can calculate time remaining
            struct tm tm;
            strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
            time_t endTime = mktime(&tm);
            
            // calculate how much time is left
            string timeRemaining = formatTimeRemaining(endTime);
            
            // start table row for this auction
            cout << "        <tr>\n";
            cout << "          <td>" << htmlEscape(itemDesc) << "</td>\n";
            cout << "          <td>$" << htmlEscape(currentBid) << "</td>\n";
            cout << "          <td>" << htmlEscape(timeRemaining) << "</td>\n";
            
            // show bid button only if user is logged in
            if (currentUserId > 0) {
                // don't let users bid on their own items
                if (sellerId == currentUserId) {
                    cout << "          <td><em>Your Item</em></td>\n";
                } else {
                    // show bid button as a link to trade.cgi with auction ID
                    cout << "          <td><a href=\"./trade.cgi?action=bid&auction_id=" 
                         << auctionId << "\" class=\"button\">Bid</a></td>\n";
                }
            }
            
            cout << "        </tr>\n";
        }
        
        cout << "      </table>\n";
    }
    
    // 8. CLEANUP AND CLOSE:
    // free result memory, close database connection, finish HTML page
    mysql_free_result(result);
    mysql_close(conn);
    
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    return 0;
}