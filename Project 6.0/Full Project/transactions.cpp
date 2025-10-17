//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: transactions.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

// transactions.cpp - Display user's transaction history
// This file shows items user is selling, purchased, bidding on, and didn't win

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() and atoi()
#include <vector>        // for vector data structures

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
    // we need database connection to check sessions and pull transaction history
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
    // transactions page requires login, so we check if user has valid session
    // if not logged in we redirect them to login page
    int currentUserId = 0;
    int sessionState = getSessionState(conn, currentUserId);
    
    // user must be logged in to see their transactions
    if (sessionState != SESSION_LOGGED_IN) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "  <head>\n";
        cout << "    <meta charset=\"UTF-8\">\n";
        cout << "    <title>Access Denied</title>\n";
        cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";
        cout << "  </head>\n";
        cout << "  <body>\n";
        cout << "    <div class=\"container\">\n";
        cout << "      <div class=\"error\">You must be logged in to access this page.</div>\n";
        cout << "      <a href=\"./index.cgi\">Login</a>\n";
        cout << "    </div>\n";
        cout << "  </body>\n";
        cout << "</html>\n";
        mysql_close(conn);
        return 0;
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
    
    // 4. RENEW SESSION ACTIVITY:
    // user is active on this page so we update last_activity to keep session alive
    string sessionId = getCookie("session");
    renewSessionActivity(conn, sessionId);
    
    // 5. START BUILDING HTML PAGE:
    // we send HTTP header first then start building the page structure
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>My Transactions</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    
    // sticky banner in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "        <h1>My Transactions</h1>\n";
    
    // NAVIGATION MENU:
    // links to other pages in the auction system
    cout << "        <div class=\"nav\">\n";
    cout << "            <a href=\"./listings.cgi\">Listings</a>\n";
    cout << "            <a href=\"./trade.cgi\">Buy/Sell</a>\n";
    cout << "            <a href=\"./transactions.cgi\">My Transactions</a>\n";
    cout << "            <a href=\"./login.cgi?action=logout\">Logout</a>\n";
    cout << "        </div>\n";
    
    // 6. SECTION 1: ITEMS I'M SELLING
    // query database for all auctions where current user is the seller
    // shows both active and closed auctions
    cout << "        <h2>Items I'm Selling</h2>\n";
    string sellingQuery = "SELECT auction_id, item_description, starting_bid, current_bid, "
                         "end_time, is_closed, winner_id FROM auctions "
                         "WHERE seller_id = " + to_string(currentUserId) +
                         " ORDER BY is_closed ASC, end_time ASC";
    
    if (mysql_query(conn, sellingQuery.c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn);
        
        // if no results, user isn't selling anything
        if (mysql_num_rows(result) == 0) {
            cout << "        <p><em>You are not selling any items.</em></p>\n";
        } else {
            // display results in a table
            cout << "        <table>\n";
            cout << "            <tr>\n";
            cout << "                <th>Item</th>\n";
            cout << "                <th>Starting Bid</th>\n";
            cout << "                <th>Current Bid</th>\n";
            cout << "                <th>Status</th>\n";
            cout << "            </tr>\n";
            
            // loop through each auction the user is selling
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                string auctionId = row[0];
                string itemDesc = row[1];
                string startingBid = row[2];
                string currentBid = row[3];
                string endTimeStr = row[4];
                int isClosed = atoi(row[5]);
                string winnerId = (row[6] != NULL) ? row[6] : "0";
                
                // convert end time string to time_t for calculation
                struct tm tm;
                strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
                time_t endTime = mktime(&tm);
                
                cout << "            <tr>\n";
                cout << "                <td>" << htmlEscape(itemDesc) << "</td>\n";
                cout << "                <td>$" << htmlEscape(startingBid) << "</td>\n";
                cout << "                <td>$" << htmlEscape(currentBid) << "</td>\n";
                cout << "                <td>";
                
                // show different status based on auction state
                if (isClosed) {
                    if (winnerId != "0") {
                        cout << "<strong>SOLD for $" << htmlEscape(currentBid) << "</strong>";
                    } else {
                        cout << "<em>Ended - No bids</em>";
                    }
                } else {
                    cout << "Active - " << htmlEscape(formatTimeRemaining(endTime)) << " left";
                }
                
                cout << "</td>\n";
                cout << "            </tr>\n";
            }
            
            cout << "        </table>\n";
        }
        
        mysql_free_result(result);
    }
    
    // 7. SECTION 2: ITEMS I PURCHASED
    // query for closed auctions where current user is the winner
    cout << "        <h2>Items I Purchased</h2>\n";
    string purchaseQuery = "SELECT auction_id, item_description, current_bid FROM auctions "
                          "WHERE is_closed = TRUE AND winner_id = " + to_string(currentUserId) +
                          " ORDER BY end_time DESC";
    
    if (mysql_query(conn, purchaseQuery.c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn);
        
        // if no results, user hasn't won anything yet
        if (mysql_num_rows(result) == 0) {
            cout << "        <p><em>You have not purchased any items yet.</em></p>\n";
        } else {
            // display purchases in a table
            cout << "        <table>\n";
            cout << "            <tr>\n";
            cout << "                <th>Item</th>\n";
            cout << "                <th>Purchase Price</th>\n";
            cout << "            </tr>\n";
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                string itemDesc = row[1];
                string purchasePrice = row[2];
                
                cout << "            <tr>\n";
                cout << "                <td>" << htmlEscape(itemDesc) << "</td>\n";
                cout << "                <td>$" << htmlEscape(purchasePrice) << "</td>\n";
                cout << "            </tr>\n";
            }
            
            cout << "        </table>\n";
        }
        
        mysql_free_result(result);
    }
    
    // 8. SECTION 3: MY CURRENT BIDS
    // query for active auctions where user has placed at least one bid
    // shows if user is currently winning or has been outbid
    cout << "        <h2>My Current Bids</h2>\n";
    string currentBidsQuery = "SELECT DISTINCT a.auction_id, a.item_description, a.current_bid, "
                             "a.end_time, a.winner_id FROM auctions a "
                             "INNER JOIN bids b ON a.auction_id = b.auction_id "
                             "WHERE b.bidder_id = " + to_string(currentUserId) +
                             " AND a.is_closed = FALSE "
                             "ORDER BY a.end_time ASC";
    
    if (mysql_query(conn, currentBidsQuery.c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn);
        
        // if no results, user isn't bidding on anything
        if (mysql_num_rows(result) == 0) {
            cout << "        <p><em>You are not currently bidding on any items.</em></p>\n";
        } else {
            // display active bids in a table
            cout << "        <table>\n";
            cout << "            <tr>\n";
            cout << "                <th>Item</th>\n";
            cout << "                <th>Current Highest Bid</th>\n";
            cout << "                <th>Time Left</th>\n";
            cout << "                <th>Status</th>\n";
            cout << "                <th>Action</th>\n";
            cout << "            </tr>\n";
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                string auctionId = row[0];
                string itemDesc = row[1];
                string currentBid = row[2];
                string endTimeStr = row[3];
                int winnerId = atoi(row[4]);
                
                // convert end time string to time_t for calculation
                struct tm tm;
                strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
                time_t endTime = mktime(&tm);
                
                cout << "            <tr>\n";
                cout << "                <td>" << htmlEscape(itemDesc) << "</td>\n";
                cout << "                <td>$" << htmlEscape(currentBid) << "</td>\n";
                cout << "                <td>" << htmlEscape(formatTimeRemaining(endTime)) << "</td>\n";
                cout << "                <td>";
                
                // check if user is currently winning or has been outbid
                if (winnerId == currentUserId) {
                    cout << "<strong style=\"color: green;\">You are winning!</strong>";
                } else {
                    cout << "<strong style=\"color: red;\">You have been outbid!</strong>";
                }
                
                cout << "</td>\n";
                cout << "                <td><a href=\"./trade.cgi?action=bid&auction_id=" 
                     << auctionId << "\" class=\"button\">Increase Bid</a></td>\n";
                cout << "            </tr>\n";
            }
            
            cout << "        </table>\n";
        }
        
        mysql_free_result(result);
    }
    
    // 9. SECTION 4: AUCTIONS I DIDN'T WIN
    // query for closed auctions where user bid but someone else won
    cout << "        <h2>Auctions I Didn't Win</h2>\n";
    string lostQuery = "SELECT DISTINCT a.auction_id, a.item_description, a.current_bid "
                      "FROM auctions a "
                      "INNER JOIN bids b ON a.auction_id = b.auction_id "
                      "WHERE b.bidder_id = " + to_string(currentUserId) +
                      " AND a.is_closed = TRUE "
                      "AND (a.winner_id IS NULL OR a.winner_id != " + to_string(currentUserId) + ") "
                      "ORDER BY a.end_time DESC";
    
    if (mysql_query(conn, lostQuery.c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn);
        
        // if no results, user hasn't lost any auctions
        if (mysql_num_rows(result) == 0) {
            cout << "        <p><em>No lost auctions.</em></p>\n";
        } else {
            // display lost auctions in a table
            cout << "        <table>\n";
            cout << "            <tr>\n";
            cout << "                <th>Item</th>\n";
            cout << "                <th>Winning Bid</th>\n";
            cout << "            </tr>\n";
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                string itemDesc = row[1];
                string winningBid = row[2];
                
                cout << "            <tr>\n";
                cout << "                <td>" << htmlEscape(itemDesc) << "</td>\n";
                cout << "                <td>$" << htmlEscape(winningBid) << "</td>\n";
                cout << "            </tr>\n";
            }
            
            cout << "        </table>\n";
        }
        
        mysql_free_result(result);
    }
    
    // 10. CLEANUP AND CLOSE:
    // close database connection and finish HTML page
    mysql_close(conn);
    
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    return 0;
}