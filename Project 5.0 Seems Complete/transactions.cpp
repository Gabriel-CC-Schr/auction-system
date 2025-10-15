// transactions.cpp - Step 3: Display user's transaction history
// This file shows items user is selling, purchased, bidding on, and didn't win

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() and atoi()
#include <vector>        // for storing results

using namespace std;

// Database connection information
const string DB_HOST = "localhost";                           // database server location
const string DB_USER = "allindragons";                        // database username
const string DB_PASS = "snogardnilla_002";                    // database password
const string DB_NAME = "cs370_section2_allindragons";         // database name

// Function to read cookies from browser
string getCookie(const string& cookieName) {
    const char* cookies = getenv("HTTP_COOKIE");  // get all cookies from browser
    if (cookies == NULL) {               // if no cookies exist
        return "";                       // return empty string
    }
    string cookieStr(cookies);           // convert to C++ string
    size_t pos = cookieStr.find(cookieName + "=");  // find our cookie
    if (pos == string::npos) {           // if cookie not found
        return "";                       // return empty string
    }
    pos += cookieName.length() + 1;      // move past "cookiename="
    size_t endPos = cookieStr.find(";", pos);  // cookies separated by semicolon
    if (endPos == string::npos) {        // if this is the last cookie
        return cookieStr.substr(pos);    // return rest of string
    }
    return cookieStr.substr(pos, endPos - pos);  // return cookie value
}

// Function to check if user session is valid
int checkSession() {
    string sessionCookie = getCookie("session");  // read session cookie
    if (sessionCookie.empty()) {         // if no session cookie exists
        return 0;                        // return 0 meaning not logged in
    }
    
    // Parse cookie value (format: userid:expiretime)
    size_t colonPos = sessionCookie.find(":");  // find the colon separator
    if (colonPos == string::npos) {      // if format is wrong
        return 0;                        // return 0 meaning invalid
    }
    
    int userId = atoi(sessionCookie.substr(0, colonPos).c_str());  // extract user ID
    time_t expireTime = atol(sessionCookie.substr(colonPos + 1).c_str());  // extract expire time
    time_t now = time(0);                // get current time
    
    if (now > expireTime) {              // if session has expired
        return 0;                        // return 0 meaning session expired
    }
    
    return userId;                       // return user ID if session is valid
}

// Function to renew session cookie (reset 5 minute timer)
void renewSessionCookie(int userId) {
    time_t now = time(0);                // get current time
    time_t expireTime = now + 300;       // expire in 5 minutes (300 seconds)
    string cookieValue = to_string(userId) + ":" + to_string(expireTime);  // format: userid:expiretime
    // Send cookie to browser - no path specified so it defaults to current directory
    cout << "Set-Cookie: session=" << cookieValue << "; HttpOnly\r\n";
}

// Function to format time remaining until auction ends
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
    // Check if user is logged in (required for this page)
    int currentUserId = checkSession();  // get user ID or 0 if not logged in
    
    if (currentUserId == 0) {            // if not logged in
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with English language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Access Denied</title>\n";  // page title
        cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";  // link to stylesheet (now CGI)
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <div class=\"container\">\n";  // container div
        cout << "      <div class=\"message error\">You must be logged in to access this page.</div>\n";  // error message
        cout << "      <a href=\"./index.cgi\">Login</a>\n";  // link to login page (now CGI)
        cout << "    </div>\n";          // close container
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
        return 0;                        // exit program
    }
    
    // Renew session cookie
    renewSessionCookie(currentUserId);   // reset 5 minute timer
    
    // Connect to MySQL database
    MYSQL* conn = mysql_init(NULL);      // initialize MySQL connection object
    if (conn == NULL) {                  // if initialization failed
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Database Error</title>\n";  // page title
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <h1>Database Error</h1>\n";  // error heading
        cout << "    <p>Failed to initialize database connection.</p>\n";  // error message
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
        return 1;                        // exit with error code
    }
    
    // Actually connect to the database
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Database Error</title>\n";  // page title
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <h1>Database Error</h1>\n";  // error heading
        cout << "    <p>Failed to connect to database: " << mysql_error(conn) << "</p>\n";  // error with details
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
        mysql_close(conn);               // close the connection
        return 1;                        // exit with error code
    }
    
    // Start building HTML page
    cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
    cout << "<!DOCTYPE html>\n";        // HTML5 document type declaration
    cout << "<html lang=\"en\">\n";     // open HTML tag with English language
    cout << "  <head>\n";               // open head section
    cout << "    <meta charset=\"UTF-8\">\n";  // set character encoding to UTF-8
    cout << "    <title>My Transactions</title>\n";  // set page title
    cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";  // link to CSS stylesheet (now CGI)
    cout << "  </head>\n";              // close head section
    cout << "  <body>\n";               // open body section
    cout << "    <div class=\"container\">\n";  // open main container div
    cout << "        <h1>My Transactions</h1>\n";  // main page heading
    
    // Show navigation menu
    cout << "        <div class=\"nav\">\n";  // open navigation div
    cout << "            <a href=\"./listings.cgi\">Listings</a>\n";  // link to listings page
    cout << "            <a href=\"./trade.cgi\">Buy/Sell</a>\n";  // link to buy/sell page
    cout << "            <a href=\"./transactions.cgi\">My Transactions</a>\n";  // link to this page
    cout << "            <a href=\"./login.cgi?action=logout\" class=\"logout\">Logout</a>\n";  // logout link
    cout << "        </div>\n";         // close navigation div
    
    // Section 1: Selling - Items user is selling (active and sold)
    cout << "        <h2>Items I'm Selling</h2>\n";  // section heading
    string sellingQuery = "SELECT auction_id, item_description, starting_bid, current_bid, "
                         "end_time, is_closed, winner_id FROM auctions "
                         "WHERE seller_id = " + to_string(currentUserId) +
                         " ORDER BY is_closed ASC, end_time ASC";
    // Query gets all auctions where current user is the seller
    // ORDER BY is_closed ASC means active auctions first, then closed ones
    
    if (mysql_query(conn, sellingQuery.c_str()) == 0) {  // if query succeeds
        MYSQL_RES* result = mysql_store_result(conn);  // get query results
        
        if (mysql_num_rows(result) == 0) {  // if no results found
            cout << "        <p><em>You are not selling any items.</em></p>\n";  // show message
        } else {
            cout << "        <table>\n";  // start table
            cout << "            <tr>\n";  // start header row
            cout << "                <th>Item</th>\n";  // column header
            cout << "                <th>Starting Bid</th>\n";  // column header
            cout << "                <th>Current Bid</th>\n";  // column header
            cout << "                <th>Status</th>\n";  // column header
            cout << "            </tr>\n";  // end header row
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {  // loop through each result row
                string auctionId = row[0];       // auction_id
                string itemDesc = row[1];        // item_description
                string startingBid = row[2];     // starting_bid
                string currentBid = row[3];      // current_bid
                string endTimeStr = row[4];      // end_time
                int isClosed = atoi(row[5]);     // is_closed (convert to integer)
                string winnerId = (row[6] != NULL) ? row[6] : "0";  // winner_id (NULL if no bids)
                
                // Convert end time string to time_t
                struct tm tm;                    // time structure
                strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);  // parse time string
                time_t endTime = mktime(&tm);    // convert to time_t
                
                cout << "            <tr>\n";    // start table row
                cout << "                <td>" << itemDesc << "</td>\n";  // item description cell
                cout << "                <td>$" << startingBid << "</td>\n";  // starting bid cell
                cout << "                <td>$" << currentBid << "</td>\n";  // current bid cell
                cout << "                <td>";  // start status cell
                
                if (isClosed) {                  // if auction has closed
                    if (winnerId != "0") {       // if there was a winner
                        cout << "<strong>SOLD for $" << currentBid << "</strong>";  // show sold message
                    } else {                     // if no bids were placed
                        cout << "<em>Ended - No bids</em>";  // show no bids message
                    }
                } else {                         // if auction is still active
                    cout << "Active - " << formatTimeRemaining(endTime) << " left";  // show time remaining
                }
                
                cout << "</td>\n";              // end status cell
                cout << "            </tr>\n";  // end table row
            }
            
            cout << "        </table>\n";       // end table
        }
        
        mysql_free_result(result);              // free result memory
    }
    
    // Section 2: Purchases - Items user won (highest bidder when auction closed)
    cout << "        <h2>Items I Purchased</h2>\n";  // section heading
    string purchaseQuery = "SELECT auction_id, item_description, current_bid FROM auctions "
                          "WHERE is_closed = TRUE AND winner_id = " + to_string(currentUserId) +
                          " ORDER BY end_time DESC";
    // Query gets closed auctions where current user is the winner
    // ORDER BY end_time DESC means most recent purchases first
    
    if (mysql_query(conn, purchaseQuery.c_str()) == 0) {  // if query succeeds
        MYSQL_RES* result = mysql_store_result(conn);  // get query results
        
        if (mysql_num_rows(result) == 0) {  // if no results
            cout << "        <p><em>You have not purchased any items yet.</em></p>\n";  // show message
        } else {
            cout << "        <table>\n";    // start table
            cout << "            <tr>\n";   // start header row
            cout << "                <th>Item</th>\n";  // column header
            cout << "                <th>Purchase Price</th>\n";  // column header
            cout << "            </tr>\n";  // end header row
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {  // loop through each result
                string itemDesc = row[1];       // item_description
                string purchasePrice = row[2];  // current_bid (which was the winning bid)
                
                cout << "            <tr>\n";   // start table row
                cout << "                <td>" << itemDesc << "</td>\n";  // item cell
                cout << "                <td>$" << purchasePrice << "</td>\n";  // price cell
                cout << "            </tr>\n";  // end table row
            }
            
            cout << "        </table>\n";      // end table
        }
        
        mysql_free_result(result);             // free result memory
    }
    
    // Section 3: Current Bids - Items user is currently bidding on (still active)
    cout << "        <h2>My Current Bids</h2>\n";  // section heading
    // Get distinct auctions where user has placed bids and auction is still open
    string currentBidsQuery = "SELECT DISTINCT a.auction_id, a.item_description, a.current_bid, "
                             "a.end_time, a.winner_id FROM auctions a "
                             "INNER JOIN bids b ON a.auction_id = b.auction_id "
                             "WHERE b.bidder_id = " + to_string(currentUserId) +
                             " AND a.is_closed = FALSE "
                             "ORDER BY a.end_time ASC";
    // Query joins auctions and bids tables to find active auctions user bid on
    // DISTINCT prevents duplicate rows if user bid multiple times on same auction
    
    if (mysql_query(conn, currentBidsQuery.c_str()) == 0) {  // if query succeeds
        MYSQL_RES* result = mysql_store_result(conn);  // get query results
        
        if (mysql_num_rows(result) == 0) {  // if no results
            cout << "        <p><em>You are not currently bidding on any items.</em></p>\n";  // show message
        } else {
            cout << "        <table>\n";    // start table
            cout << "            <tr>\n";   // start header row
            cout << "                <th>Item</th>\n";  // column header
            cout << "                <th>Current Highest Bid</th>\n";  // column header
            cout << "                <th>Time Left</th>\n";  // column header
            cout << "                <th>Status</th>\n";  // column header
            cout << "                <th>Action</th>\n";  // column header
            cout << "            </tr>\n";  // end header row
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {  // loop through each result
                string auctionId = row[0];      // auction_id
                string itemDesc = row[1];       // item_description
                string currentBid = row[2];     // current_bid
                string endTimeStr = row[3];     // end_time
                int winnerId = atoi(row[4]);    // winner_id (current high bidder)
                
                // Convert end time string to time_t
                struct tm tm;                   // time structure
                strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);  // parse time
                time_t endTime = mktime(&tm);   // convert to time_t
                
                cout << "            <tr>\n";   // start table row
                cout << "                <td>" << itemDesc << "</td>\n";  // item cell
                cout << "                <td>$" << currentBid << "</td>\n";  // current bid cell
                cout << "                <td>" << formatTimeRemaining(endTime) << "</td>\n";  // time left cell
                cout << "                <td>";  // start status cell
                
                // Check if user is currently winning
                if (winnerId == currentUserId) {  // if current user is winning
                    cout << "<strong style=\"color: green;\">You are winning!</strong>";  // winning message
                } else {                         // if someone else is winning
                    cout << "<strong style=\"color: red;\">You have been outbid!</strong>";  // outbid message
                }
                
                cout << "</td>\n";              // end status cell
                cout << "                <td><a href=\"./trade.cgi?action=bid&auction_id=" 
                     << auctionId << "\" class=\"button\">Increase Bid</a></td>\n";  // bid button
                cout << "            </tr>\n";  // end table row
            }
            
            cout << "        </table>\n";      // end table
        }
        
        mysql_free_result(result);             // free result memory
    }
    
    // Section 4: Didn't Win - Items user bid on but lost
    cout << "        <h2>Auctions I Didn't Win</h2>\n";  // section heading
    // Get closed auctions where user placed bids but someone else won
    string lostQuery = "SELECT DISTINCT a.auction_id, a.item_description, a.current_bid "
                      "FROM auctions a "
                      "INNER JOIN bids b ON a.auction_id = b.auction_id "
                      "WHERE b.bidder_id = " + to_string(currentUserId) +
                      " AND a.is_closed = TRUE "
                      "AND (a.winner_id IS NULL OR a.winner_id != " + to_string(currentUserId) + ") "
                      "ORDER BY a.end_time DESC";
    // Query finds closed auctions where user bid but didn't win
    // winner_id IS NULL means no one won (no bids met reserve)
    // winner_id != currentUserId means someone else won
    
    if (mysql_query(conn, lostQuery.c_str()) == 0) {  // if query succeeds
        MYSQL_RES* result = mysql_store_result(conn);  // get query results
        
        if (mysql_num_rows(result) == 0) {  // if no results
            cout << "        <p><em>No lost auctions.</em></p>\n";  // show message
        } else {
            cout << "        <table>\n";    // start table
            cout << "            <tr>\n";   // start header row
            cout << "                <th>Item</th>\n";  // column header
            cout << "                <th>Winning Bid</th>\n";  // column header
            cout << "            </tr>\n";  // end header row
            
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {  // loop through each result
                string itemDesc = row[1];       // item_description
                string winningBid = row[2];     // current_bid (final winning bid)
                
                cout << "            <tr>\n";   // start table row
                cout << "                <td>" << itemDesc << "</td>\n";  // item cell
                cout << "                <td>$" << winningBid << "</td>\n";  // winning bid cell
                cout << "            </tr>\n";  // end table row
            }
            
            cout << "        </table>\n";      // end table
        }
        
        mysql_free_result(result);             // free result memory
    }
    
    mysql_close(conn);                         // close database connection
    
    cout << "    </div>\n";                   // close container div
    cout << "  </body>\n";                    // close body section
    cout << "</html>\n";                      // close HTML tag
    
    return 0;                                  // exit successfully
}