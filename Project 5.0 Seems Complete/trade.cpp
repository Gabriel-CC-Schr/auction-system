// trade.cpp - Step 4: Trade System for buying and selling
// This file handles placing bids and creating new auctions

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <sstream>       // for string stream operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() and atoi()

using namespace std;

// Database connection information
const string DB_HOST = "localhost";                           // database server location
const string DB_USER = "allindragons";                        // database username
const string DB_PASS = "snogardnilla_002";                    // database password
const string DB_NAME = "cs370_section2_allindragons";         // database name

// Function to decode URL-encoded strings (convert %20 to space, etc.)
string urlDecode(const string& str) {
    string result = "";                  // this will hold our decoded string
    for (size_t i = 0; i < str.length(); i++) {  // loop through each character
        if (str[i] == '+') {             // plus signs become spaces
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {  // % followed by two hex digits
            // Convert hex to character (example: %20 becomes space)
            int hexValue;                // will hold the numeric value
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hexValue);  // read hex value
            result += static_cast<char>(hexValue);  // convert to character
            i += 2;                      // skip the two hex digits we just processed
        } else {
            result += str[i];            // normal character, just copy it
        }
    }
    return result;                       // return the decoded string
}

// Function to parse query string (for GET) or post data (for POST)
string getValue(const string& data, const string& fieldName) {
    size_t pos = data.find(fieldName + "=");  // find where field appears
    if (pos == string::npos) {           // if field not found
        return "";                       // return empty string
    }
    pos += fieldName.length() + 1;       // move past "fieldname="
    size_t endPos = data.find("&", pos);  // find next field (marked by &)
    string value;                        // will hold the field value
    if (endPos == string::npos) {        // if this is the last field
        value = data.substr(pos);        // take everything to the end
    } else {
        value = data.substr(pos, endPos - pos);  // take until next &
    }
    return urlDecode(value);             // decode and return the value
}

// Function to escape special characters for SQL (prevent SQL injection)
string escapeSQL(MYSQL* conn, const string& str) {
    char* escaped = new char[str.length() * 2 + 1];  // allocate enough space
    mysql_real_escape_string(conn, escaped, str.c_str(), str.length());  // escape the string
    string result(escaped);              // convert back to C++ string
    delete[] escaped;                    // free the memory we allocated
    return result;                       // return escaped string
}

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

int main() {
    // Check if user is logged in (required for this page)
    int currentUserId = checkSession();  // get user ID or 0 if not logged in
    
    if (currentUserId == 0) {            // if not logged in
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='message error'>You must be logged in to access this page.</div>";
        cout << "<a href='./index.cgi'>Login</a>";
        cout << "</div></body></html>";
        return 0;
    }
    
    // Renew session cookie
    renewSessionCookie(currentUserId);   // reset 5 minute timer
    
    // Get request method (GET or POST)
    const char* requestMethod = getenv("REQUEST_METHOD");
    string method = (requestMethod != NULL) ? string(requestMethod) : "GET";
    
    // Get query string (for GET) or POST data
    string requestData = "";
    if (method == "GET") {
        const char* queryString = getenv("QUERY_STRING");
        if (queryString != NULL) {
            requestData = string(queryString);
        }
    } else if (method == "POST") {
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
    
    // Get action from request data
    string action = getValue(requestData, "action");
    
    // Connect to MySQL database
    MYSQL* conn = mysql_init(NULL);      // initialize MySQL connection object
    if (conn == NULL) {                  // if initialization failed
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Error</h1>";
        cout << "<p>Failed to initialize database connection.</p></body></html>";
        return 1;
    }
    
    // Actually connect to the database
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Error</h1>";
        cout << "<p>Failed to connect to database: " << mysql_error(conn) << "</p></body></html>";
        mysql_close(conn);
        return 1;
    }
    
    // Handle different actions
    if (action == "submit_bid" && method == "POST") {
        // Process bid submission
        string auctionIdStr = getValue(requestData, "auction_id");
        string bidAmountStr = getValue(requestData, "bid_amount");
        
        int auctionId = atoi(auctionIdStr.c_str());
        double bidAmount = atof(bidAmountStr.c_str());
        
        // Validate inputs
        if (auctionId <= 0 || bidAmount <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Invalid bid information.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Get auction details
        string checkQuery = "SELECT seller_id, current_bid, is_closed FROM auctions WHERE auction_id = " + 
                          to_string(auctionId);
        if (mysql_query(conn, checkQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Database error.</div>";
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
            cout << "<div class='message error'>Auction not found.</div>";
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
        
        // Check if user is trying to bid on own item
        if (sellerId == currentUserId) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>You cannot bid on your own items.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Check if auction is closed
        if (isClosed) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>This auction has ended.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Check if bid is higher than current bid
        if (bidAmount <= currentBid) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Bid must be higher than current bid of $" 
                 << currentBid << "</div>";
            cout << "<a href='./trade.cgi?action=bid&auction_id=" << auctionId << "'>Try Again</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Insert the bid
        string insertBidQuery = "INSERT INTO bids (auction_id, bidder_id, bid_amount) VALUES (" +
                               to_string(auctionId) + ", " + to_string(currentUserId) + ", " +
                               to_string(bidAmount) + ")";
        if (mysql_query(conn, insertBidQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Failed to place bid.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Update auction with new current bid and winner
        string updateQuery = "UPDATE auctions SET current_bid = " + to_string(bidAmount) +
                           ", winner_id = " + to_string(currentUserId) +
                           " WHERE auction_id = " + to_string(auctionId);
        mysql_query(conn, updateQuery.c_str());
        
        // Success message
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='message success'>Bid placed successfully!</div>";
        cout << "<p>Your bid of $" << bidAmount << " has been recorded.</p>";
        cout << "<a href='./listings.cgi'>View All Listings</a> | ";
        cout << "<a href='./transactions.cgi'>View My Transactions</a>";
        cout << "</div></body></html>";
        
    } else if (action == "submit_sell" && method == "POST") {
        // Process new auction submission
        string itemDesc = getValue(requestData, "item_description");
        string startingBidStr = getValue(requestData, "starting_bid");
        
        double startingBid = atof(startingBidStr.c_str());
        
        // Validate inputs
        if (itemDesc.empty() || startingBid <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>All fields are required and starting bid must be positive.</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Escape description to prevent SQL injection
        string safeDesc = escapeSQL(conn, itemDesc);
        
        // Calculate end time (168 hours = 7 days from now)
        time_t now = time(0);            // get current time
        time_t endTime = now + (168 * 3600);  // add 168 hours (in seconds)
        
        // Convert times to MySQL format
        char startTimeStr[20];
        char endTimeStr[20];
        strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%d %H:%M:%S", localtime(&endTime));
        
        // Insert new auction
        string insertQuery = "INSERT INTO auctions (seller_id, item_description, starting_bid, "
                           "current_bid, start_time, end_time) VALUES (" +
                           to_string(currentUserId) + ", '" + safeDesc + "', " +
                           to_string(startingBid) + ", " + to_string(startingBid) + ", '" +
                           string(startTimeStr) + "', '" + string(endTimeStr) + "')";
        
        if (mysql_query(conn, insertQuery.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Failed to create auction: " << mysql_error(conn) << "</div>";
            cout << "<a href='./trade.cgi'>Go Back</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Success message
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
        cout << "<div class='container'>";
        cout << "<div class='message success'>Auction created successfully!</div>";
        cout << "<p>Your item will be listed for 7 days.</p>";
        cout << "<a href='./listings.cgi'>View All Listings</a> | ";
        cout << "<a href='./transactions.cgi'>View My Transactions</a>";
        cout << "</div></body></html>";
        
    } else if (action == "bid") {
        // Show bid form for specific auction
        string auctionIdStr = getValue(requestData, "auction_id");
        int auctionId = atoi(auctionIdStr.c_str());
        
        if (auctionId <= 0) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Invalid auction ID.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Get auction details
        string query = "SELECT item_description, current_bid, seller_id FROM auctions WHERE auction_id = " +
                      to_string(auctionId) + " AND is_closed = FALSE";
        if (mysql_query(conn, query.c_str())) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>Database error.</div>";
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
            cout << "<div class='message error'>Auction not found or has ended.</div>";
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
        
        // Check if user is seller
        if (sellerId == currentUserId) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<html><head><link rel='stylesheet' href='./style.cgi'></head><body>";
            cout << "<div class='container'>";
            cout << "<div class='message error'>You cannot bid on your own items.</div>";
            cout << "<a href='./listings.cgi'>View Listings</a>";
            cout << "</div></body></html>";
            mysql_close(conn);
            return 0;
        }
        
        // Display bid form
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang='en'>\n";
        cout << "<head>\n";
        cout << "    <meta charset='UTF-8'>\n";
        cout << "    <title>Place Bid</title>\n";
        cout << "    <link rel='stylesheet' href='./style.cgi'>\n";
        cout << "</head>\n";
        cout << "<body>\n";
        cout << "    <div class='container'>\n";
        cout << "        <h1>Place Your Bid</h1>\n";
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings</a>\n";
        cout << "            <a href='./trade.cgi'>Buy/Sell</a>\n";
        cout << "            <a href='./transactions.cgi'>My Transactions</a>\n";
        cout << "            <a href='./login.cgi?action=logout' class='logout'>Logout</a>\n";
        cout << "        </div>\n";
        cout << "        <div class='auction-item'>\n";
        cout << "            <h3>Item: " << itemDesc << "</h3>\n";
        cout << "            <p><strong>Current Bid:</strong> $" << currentBid << "</p>\n";
        cout << "        </div>\n";
        cout << "        <div class='form-section'>\n";
        cout << "            <h2>Enter Your Bid</h2>\n";
        cout << "            <form action='./trade.cgi' method='POST'>\n";
        cout << "                <input type='hidden' name='action' value='submit_bid'>\n";
        cout << "                <input type='hidden' name='auction_id' value='" << auctionId << "'>\n";
        cout << "                <label for='bid_amount'>Bid Amount ($):</label>\n";
        cout << "                <input type='number' id='bid_amount' name='bid_amount' step='0.01' min='" 
             << currentBid << "' required>\n";
        cout << "                <p><em>Your bid must be higher than $" << currentBid << "</em></p>\n";
        cout << "                <button type='submit'>Place Bid</button>\n";
        cout << "            </form>\n";
        cout << "        </div>\n";
        cout << "    </div>\n";
        cout << "</body>\n";
        cout << "</html>\n";
        
    } else {
        // Show main trade page with sell form and auction dropdown
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang='en'>\n";
        cout << "<head>\n";
        cout << "    <meta charset='UTF-8'>\n";
        cout << "    <title>Buy/Sell</title>\n";
        cout << "    <link rel='stylesheet' href='./style.cgi'>\n";
        cout << "</head>\n";
        cout << "<body>\n";
        cout << "    <div class='container'>\n";
        cout << "        <h1>Buy or Sell Items</h1>\n";
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings</a>\n";
        cout << "            <a href='./trade.cgi'>Buy/Sell</a>\n";
        cout << "            <a href='./transactions.cgi'>My Transactions</a>\n";
        cout << "            <a href='./login.cgi?action=logout' class='logout'>Logout</a>\n";
        cout << "        </div>\n";
        
        // Sell section
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
        
        // Buy section - show dropdown of available auctions
        cout << "        <div class='form-section'>\n";
        cout << "            <h2>Bid on an Item</h2>\n";
        cout << "            <p>Select an auction from the dropdown below or <a href='./listings.cgi'>view all listings</a>.</p>\n";
        
        // Get all open auctions (not owned by current user)
        string auctionQuery = "SELECT auction_id, item_description, current_bid FROM auctions "
                            "WHERE is_closed = FALSE AND seller_id != " + to_string(currentUserId) +
                            " ORDER BY end_time ASC";
        
        if (mysql_query(conn, auctionQuery.c_str()) == 0) {
            MYSQL_RES* result = mysql_store_result(conn);
            
            if (mysql_num_rows(result) > 0) {
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
                    // Truncate description if too long
                    if (itemDesc.length() > 50) {
                        itemDesc = itemDesc.substr(0, 47) + "...";
                    }
                    cout << "                    <option value='" << auctionId << "'>" 
                         << itemDesc << " (Current: $" << currentBid << ")</option>\n";
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
    
    mysql_close(conn);                   // close database connection
    return 0;                            // exit successfully
}