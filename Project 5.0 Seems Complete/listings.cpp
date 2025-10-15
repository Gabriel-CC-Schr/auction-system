// listings.cpp - Step 5: Display all auction listings
// This file shows all open auctions, ordered by ending soonest

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() and atoi()

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
    // Check if user is logged in FIRST (before any output)
    int currentUserId = checkSession();  // get user ID or 0 if not logged in
    
    // If logged in, renew the session cookie BEFORE any HTML output
    if (currentUserId > 0) {
        renewSessionCookie(currentUserId);  // reset 5 minute timer and send cookie header
    }
    
    // Connect to MySQL database
    MYSQL* conn = mysql_init(NULL);      // initialize MySQL connection object
    if (conn == NULL) {                  // if initialization failed
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with English language
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
    
    // First, update any auctions that have expired
    string updateQuery = "UPDATE auctions SET is_closed = TRUE WHERE end_time < NOW() AND is_closed = FALSE";
    mysql_query(conn, updateQuery.c_str());  // execute update (we don't need to check result)
    
    // Query to get all open auctions, ordered by end time (soonest first)
    string listQuery = "SELECT auction_id, seller_id, item_description, starting_bid, "
                      "current_bid, start_time, end_time, winner_id "
                      "FROM auctions WHERE is_closed = FALSE "
                      "ORDER BY end_time ASC";  // ASC means ascending (soonest first)
    
    if (mysql_query(conn, listQuery.c_str())) {  // if query fails
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Database Error</title>\n";  // page title
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <h1>Database Error</h1>\n";  // error heading
        cout << "    <p>Failed to fetch auctions: " << mysql_error(conn) << "</p>\n";  // error details
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
        mysql_close(conn);               // close database connection
        return 1;                        // exit with error code
    }
    
    MYSQL_RES* result = mysql_store_result(conn);  // get query results
    
    // Start building HTML page
    cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
    cout << "<!DOCTYPE html>\n";        // HTML5 document type declaration
    cout << "<html lang=\"en\">\n";     // open HTML tag with English language
    cout << "  <head>\n";               // open head section
    cout << "    <meta charset=\"UTF-8\">\n";  // set character encoding to UTF-8
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";  // make responsive for mobile
    cout << "    <title>Auction Listings</title>\n";  // set page title
    cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";  // link to CSS stylesheet (now CGI)
    cout << "  </head>\n";              // close head section
    cout << "  <body>\n";               // open body section
    cout << "    <div class=\"container\">\n";  // open main container div
    cout << "      <h1>Current Auctions</h1>\n";  // main page heading
    
    // Show navigation menu
    cout << "      <div class=\"nav\">\n";  // open navigation div
    cout << "        <a href=\"./listings.cgi\">Listings</a>\n";  // link to listings page
    
    if (currentUserId > 0) {             // if user is logged in
        cout << "        <a href=\"./trade.cgi\">Buy/Sell</a>\n";  // link to buy/sell page
        cout << "        <a href=\"./transactions.cgi\">My Transactions</a>\n";  // link to transactions page
        cout << "        <a href=\"./login.cgi?action=logout\" class=\"logout\">Logout</a>\n";  // logout link
    } else {                             // if user is not logged in
        cout << "        <a href=\"./index.cgi\">Login/Register</a>\n";  // link to login/register page
    }
    
    cout << "      </div>\n";           // close navigation div
    
    // Check if there are any auctions
    if (mysql_num_rows(result) == 0) {   // if no auctions found
        cout << "      <p>No active auctions at this time.</p>\n";  // display message
    } else {
        // Display auctions in a table
        cout << "      <table>\n";      // open table element
        cout << "        <tr>\n";       // open table header row
        cout << "          <th>Item Description</th>\n";  // column header for item description
        cout << "          <th>Current Bid</th>\n";  // column header for current bid
        cout << "          <th>Time Remaining</th>\n";  // column header for time remaining
        if (currentUserId > 0) {         // if logged in, show bid button column
            cout << "          <th>Action</th>\n";  // column header for action button
        }
        cout << "        </tr>\n";      // close header row
        
        // Loop through each auction result
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {  // while there are more rows
            int auctionId = atoi(row[0]);        // auction_id (convert string to integer)
            int sellerId = atoi(row[1]);         // seller_id (convert string to integer)
            string itemDesc = row[2];            // item_description
            string startingBid = row[3];         // starting_bid
            string currentBid = row[4];          // current_bid
            string startTime = row[5];           // start_time
            string endTimeStr = row[6];          // end_time
            
            // Convert end time string to time_t for calculation
            struct tm tm;                        // time structure to hold parsed time
            strptime(endTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);  // parse time string
            time_t endTime = mktime(&tm);        // convert to time_t (seconds since 1970)
            
            // Calculate time remaining
            string timeRemaining = formatTimeRemaining(endTime);
            
            // Start table row
            cout << "        <tr>\n";    // open table row
            cout << "          <td>" << itemDesc << "</td>\n";  // display item description
            cout << "          <td>$" << currentBid << "</td>\n";  // display current bid with dollar sign
            cout << "          <td>" << timeRemaining << "</td>\n";  // display time remaining
            
            // Show bid button only if logged in
            if (currentUserId > 0) {
                // Don't show bid button if user is the seller
                if (sellerId == currentUserId) {
                    cout << "          <td><em>Your Item</em></td>\n";  // show "Your Item" if user owns this
                } else {
                    // Show bid button as a link to trade.cgi
                    cout << "          <td><a href=\"./trade.cgi?action=bid&auction_id=" 
                         << auctionId << "\" class=\"button\">Bid</a></td>\n";  // bid button/link
                }
            }
            
            cout << "        </tr>\n";  // close table row
        }
        
        cout << "      </table>\n";     // close table element
    }
    
    mysql_free_result(result);           // free result memory
    mysql_close(conn);                   // close database connection
    
    cout << "    </div>\n";             // close container div
    cout << "  </body>\n";              // close body section
    cout << "</html>\n";                // close HTML tag
    
    return 0;                            // exit successfully
}