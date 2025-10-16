//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: trade.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

// trade.cpp - Trade System for buying and selling
// This file handles placing bids and creating new auctions

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <sstream>       // for string stream operations
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

// CONVERTING WEIRD URL GOBBLEDEYGOOK BACK INTO CHARACTERS:
// this function just decodes url %xx stuff back into characters
string urlDecode(const string& str) {
    string result = "";
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '+') {
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {
            int hexValue;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hexValue);
            result += static_cast<char>(hexValue);
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
string getValue(const string& data, const string& fieldName) {
    size_t pos = data.find(fieldName + "=");
    if (pos == string::npos) {
        return "";
    }
    pos += fieldName.length() + 1;
    size_t endPos = data.find("&", pos);
    string value;
    if (endPos == string::npos) {
        value = data.substr(pos);
    } else {
        value = data.substr(pos, endPos - pos);
    }
    return urlDecode(value);
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

int main() {

    // 1. CONNECT TO DATABASE FIRST:
    // we need database connection to check sessions, process bids, and create auctions
    // if connection fails we show a basic error page
    MYSQL* conn = mysql_init(NULL);
    if (conn == NULL) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Error</h1>";
        cout << "<p>Failed to initialize database connection.</p></body></html>";
        return 1;
    }
    
    // actually connect to the database using our credentials
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Error</h1>";
        cout << "<p>Failed to connect to database: " << htmlEscape(mysql_error(conn)) << "</p></body></html>";
        mysql_close(conn);
        return 1;
    }
    
    // 2. CHECK SESSION STATE - LOGIN REQUIRED:
    // trade page requires login because you need account to buy or sell
    // if not logged in we show error and link to login page
    int currentUserId = 0;
    int sessionState = getSessionState(conn, currentUserId);
    
    if (sessionState != SESSION_LOGGED_IN) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='error'>You must be logged in to access this page.</div>";
        cout << "<a href='./index.cgi'>Login</a>";
        cout << "</div></body></html>";
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
    
    // 5. DETERMINE REQUEST METHOD AND GET DATA:
    // figure out if this is a GET request (viewing forms) or POST request (submitting forms)
    // GET data comes from URL after "?", POST data comes from form submission
    const char* requestMethod = getenv("REQUEST_METHOD");
    string method = (requestMethod != NULL) ? string(requestMethod) : "GET";
    
    string requestData = "";
    if (method == "GET") {
        // GET: read from QUERY_STRING environment variable
        const char* queryString = getenv("QUERY_STRING");
        if (queryString != NULL) {
            requestData = string(queryString);
        }
    } else if (method == "POST") {
        // POST: read from standard input (cin) based on CONTENT_LENGTH
        const char* contentLength = getenv("CONTENT_LENGTH");
        if (contentLength != NULL) {
            int length = atoi(contentLength);
            char* buffer = new char[length + 1];
            cin.read(buffer, length);
            buffer[length] = '\0';
            requestData = string(buffer);
            delete[] buffer;
        }
    }
    
    // 6. EXTRACT ACTION PARAMETER:
    // action tells us what the user wants to do: submit_bid, submit_sell, or bid (show form)
    string action = getValue(requestData, "action");
    
    // 7. HANDLE BID SUBMISSION:
    // when user submits a bid form we process it here
    if (action == "submit_bid" && method == "POST") {
        string auctionIdStr = getValue(requestData, "auction_id");
        string bidAmountStr = getValue(requestData, "bid_amount");
        
        // convert strings to numbers
        int auctionId = atoi(auctionIdStr.c_str());
        double bidAmount = atof(bidAmountStr.c_str());
        
        // validate input: auction ID and bid amount must be positive
        if (auctionId <= 0 || bidAmount <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Invalid bid information.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // look up auction details to validate bid
        string checkQuery = "SELECT seller_id, current_bid, is_closed FROM auctions WHERE auction_id = " + 
                          to_string(auctionId);
        if (mysql_query(conn, checkQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Database error.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        MYSQL_RES* result = mysql_store_result(conn);
        if (mysql_num_rows(result) == 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Auction not found.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_free_result(result);
            mysql_close(conn);
            return 0;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        int sellerId = atoi(row[0]);
        double currentBid = atof(row[1]);
        int isClosed = atoi(row[2]);
        mysql_free_result(result);
        
        // BID VALIDATION CHECKS:
        // check 1: users can't bid on their own items
        if (sellerId == currentUserId) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>You cannot bid on your own items.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // check 2: auction must still be open
        if (isClosed) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>This auction has ended.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // check 3: new bid must be higher than current bid
        if (bidAmount <= currentBid) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Bid must be higher than current bid of $" 
                 << to_string(currentBid) << "</div>";
            cout << "<a href='./trade.cgi?action=bid&auction_id=" << auctionId << "'>Try Again</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // BID ACCEPTED - RECORD IT:
        // insert new bid into bids table
        string insertBidQuery = "INSERT INTO bids (auction_id, bidder_id, bid_amount) VALUES (" +
                               to_string(auctionId) + ", " + to_string(currentUserId) + ", " +
                               to_string(bidAmount) + ")";
        if (mysql_query(conn, insertBidQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Failed to place bid.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // update auction with new current bid and winner
        string updateQuery = "UPDATE auctions SET current_bid = " + to_string(bidAmount) +
                           ", winner_id = " + to_string(currentUserId) +
                           " WHERE auction_id = " + to_string(auctionId);
        mysql_query(conn, updateQuery.c_str());
        
        // show success message
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='success'>Bid placed successfully!</div>";
        cout << "<p>Your bid of $" << to_string(bidAmount) << " has been recorded.</p>";
        cout << "<a href='./listings.cgi'>View All Listings</a> | ";
        cout << "<a href='./transactions.cgi'>View My Transactions</a>";
        cout << "</div></body></html>";
        
    // 8. HANDLE SELL SUBMISSION:
    // when user submits form to create new auction we process it here
    } else if (action == "submit_sell" && method == "POST") {
        string itemDesc = getValue(requestData, "item_description");
        string startingBidStr = getValue(requestData, "starting_bid");
        
        double startingBid = atof(startingBidStr.c_str());
        
        // validate input: description required and starting bid must be positive
        if (itemDesc.empty() || startingBid <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>All fields are required and starting bid must be positive.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // escape item description to prevent SQL injection
        string safeDesc = escapeSQL(conn, itemDesc);
        
        // CALCULATE AUCTION TIMES:
        // start time is now, end time is 7 days (168 hours) from now
        time_t now = time(0);
        time_t endTime = now + (168 * 3600);  // 168 hours = 7 days
        
        // format times as MySQL datetime strings
        char startTimeStr[20];
        char endTimeStr[20];
        strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%d %H:%M:%S", localtime(&endTime));
        
        // INSERT NEW AUCTION INTO DATABASE:
        // current_bid starts at same value as starting_bid since no one bid yet
        string insertQuery = "INSERT INTO auctions (seller_id, item_description, starting_bid, "
                           "current_bid, start_time, end_time) VALUES (" +
                           to_string(currentUserId) + ", '" + safeDesc + "', " +
                           to_string(startingBid) + ", " + to_string(startingBid) + ", '" +
                           string(startTimeStr) + "', '" + string(endTimeStr) + "')";
        
        if (mysql_query(conn, insertQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Failed to create auction: " << htmlEscape(mysql_error(conn)) << "</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // show success message
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='success'>Auction created successfully!</div>";
        cout << "<p>Your item will be listed for 7 days.</p>";
        cout << "<a href='./listings.cgi'>View All Listings</a> | ";
        cout << "<a href='./transactions.cgi'>View My Transactions</a>";
        cout << "</div></body></html>";
        
    // 9. SHOW BID FORM FOR SPECIFIC AUCTION:
    // when user clicks "Bid" button on listings page they come here
    } else if (action == "bid") {
        string auctionIdStr = getValue(requestData, "auction_id");
        int auctionId = atoi(auctionIdStr.c_str());
        
        // validate auction ID
        if (auctionId <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Invalid auction ID.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // look up auction details to show in form
        string query = "SELECT item_description, current_bid, seller_id FROM auctions WHERE auction_id = " +
                      to_string(auctionId) + " AND is_closed = FALSE";
        if (mysql_query(conn, query.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Database error.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        MYSQL_RES* result = mysql_store_result(conn);
        if (mysql_num_rows(result) == 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>Auction not found or has ended.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_free_result(result);
            mysql_close(conn);
            return 0;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        string itemDesc = row[0];
        string currentBid = row[1];
        int sellerId = atoi(row[2]);
        mysql_free_result(result);
        
        // don't let users bid on their own items
        if (sellerId == currentUserId) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='error'>You cannot bid on your own items.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // DISPLAY BID FORM:
        // show form where user can enter their bid amount
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang='en'>\n";
        cout << "<head>\n";
        cout << "    <meta charset='UTF-8'>\n";
        cout << "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
        cout << "    <title>Place Bid</title>\n";
        cout << "    <link rel='stylesheet' href='./style.cgi'>\n";
        cout << "</head>\n";
        cout << "<body>\n";
        
        // sticky banner in top-left that tells user their login status
        cout << "    <div class='status'><em>" << htmlEscape(statusMessage) << "</em></div>\n";
        
        // main content container
        cout << "    <div class='container'>\n";
        cout << "        <h1>Place Your Bid</h1>\n";
        
        // navigation menu
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings</a>\n";
        cout << "            <a href='./trade.cgi'>Buy/Sell</a>\n";
        cout << "            <a href='./transactions.cgi'>My Transactions</a>\n";
        cout << "            <a href='./login.cgi?action=logout'>Logout</a>\n";
        cout << "        </div>\n";
        
        // show auction details
        cout << "        <div class='auction-item'>\n";
        cout << "            <h3>Item: " << htmlEscape(itemDesc) << "</h3>\n";
        cout << "            <p><strong>Current Bid:</strong> $" << htmlEscape(currentBid) << "</p>\n";
        cout << "        </div>\n";
        
        // bid form
        cout << "        <div class='form-section'>\n";
        cout << "            <h2>Enter Your Bid</h2>\n";
        cout << "            <form action='./trade.cgi' method='POST'>\n";
        cout << "                <input type='hidden' name='action' value='submit_bid'>\n";
        cout << "                <input type='hidden' name='auction_id' value='" << auctionId << "'>\n";
        cout << "                <label for='bid_amount'>Bid Amount ($):</label>\n";
        cout << "                <input type='number' id='bid_amount' name='bid_amount' step='0.01' min='" 
             << htmlEscape(currentBid) << "' required>\n";
        cout << "                <p><em>Your bid must be higher than $" << htmlEscape(currentBid) << "</em></p>\n";
        cout << "                <button type='submit'>Place Bid</button>\n";
        cout << "            </form>\n";
        cout << "        </div>\n";
        cout << "    </div>\n";
        cout << "</body>\n";
        cout << "</html>\n";
        
    // 10. DEFAULT VIEW - SHOW BUY/SELL PAGE:
    // if no specific action, show main trade page with both sell form and buy dropdown
    } else {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang='en'>\n";
        cout << "<head>\n";
        cout << "    <meta charset='UTF-8'>\n";
        cout << "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
        cout << "    <title>Buy/Sell</title>\n";
        cout << "    <link rel='stylesheet' href='./style.cgi'>\n";
        cout << "</head>\n";
        cout << "<body>\n";
        
        // sticky banner in top-left that tells user their login status
        cout << "    <div class='status'><em>" << htmlEscape(statusMessage) << "</em></div>\n";
        
        // main content container
        cout << "    <div class='container'>\n";
        cout << "        <h1>Buy or Sell Items</h1>\n";
        
        // navigation menu
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings</a>\n";
        cout << "            <a href='./trade.cgi'>Buy/Sell</a>\n";
        cout << "            <a href='./transactions.cgi'>My Transactions</a>\n";
        cout << "            <a href='./login.cgi?action=logout'>Logout</a>\n";
        cout << "        </div>\n";
        
        // SELL SECTION:
        // form to create new auction
        cout << "        <div class='form-section'>\n";
        cout << "            <h2>Sell an Item</h2>\n";
        cout << "            <form action='./trade.cgi' method='POST'>\n";
        cout << "                <input type='hidden' name='action' value='submit_sell'>\n";
        cout << "                <label for='item_description'>Item Description:</label>\n";
        cout << "                <textarea id='item_description' name='item_description' required></textarea>\n";
        cout << "                <label for='starting_bid'>Starting Bid ($):</label>\n";
        cout << "                <input type='number' id='starting_bid' name='starting_bid' step='0.01' min='0.01' required>\n";
        cout << "                <p><em>Auction will last 7 days (168 hours) starting now.</em></p>\n";
        cout << "                <button type='submit'>Create Auction</button>\n";
        cout << "            </form>\n";
        cout << "        </div>\n";
        
        // BUY SECTION:
        // dropdown to select auction to bid on
        cout << "        <div class='form-section'>\n";
        cout << "            <h2>Bid on an Item</h2>\n";
        cout << "            <p>Select an auction from the dropdown below or <a href='./listings.cgi'>view all listings</a>.</p>\n";
        
        // query for auctions where current user is NOT the seller
        string auctionQuery = "SELECT auction_id, item_description, current_bid FROM auctions "
                            "WHERE is_closed = FALSE AND seller_id != " + to_string(currentUserId) +
                            " ORDER BY end_time ASC";
        
        if (mysql_query(conn, auctionQuery.c_str()) == 0) {
            MYSQL_RES* result = mysql_store_result(conn);
            
            if (mysql_num_rows(result) > 0) {
                // show dropdown with available auctions
                cout << "            <form action='./trade.cgi' method='GET'>\n";
                cout << "                <input type='hidden' name='action' value='bid'>\n";
                cout << "                <label for='auction_id'>Select Auction:</label>\n";
                cout << "                <select id='auction_id' name='auction_id' required>\n";
                cout << "                    <option value=''>-- Choose an auction --</option>\n";
                
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(result))) {
                    string auctionId = row[0];
                    string itemDesc = row[1];
                    string currentBid = row[2];
                    
                    // truncate long descriptions so dropdown looks nice
                    if (itemDesc.length() > 50) {
                        itemDesc = itemDesc.substr(0, 47) + "...";
                    }
                    
                    cout << "                    <option value='" << htmlEscape(auctionId) << "'>" 
                         << htmlEscape(itemDesc) << " (Current: $" << htmlEscape(currentBid) << ")</option>\n";
                }
                
                cout << "                </select>\n";
                cout << "                <button type='submit'>Go to Bid</button>\n";
                cout << "            </form>\n";
            } else {
                cout << "            <p><em>No auctions available for bidding at this time.</em></p>\n";
            }
            
            mysql_free_result(result);
        }
        
        cout << "        </div>\n";
        cout << "    </div>\n";
        cout << "</body>\n";
        cout << "</html>\n";
    }
    
    // 11. CLEANUP AND CLOSE:
    // close database connection
    mysql_close(conn);
    return 0;
}