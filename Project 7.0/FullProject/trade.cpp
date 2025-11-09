//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: trade.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

// trade.cpp - this is where all the buying and selling happens
// This file handles placing bids and creating new auctions

#include <iostream>
#include <string>
#include <ctime>
#include <sstream>
#include <mysql/mysql.h>
#include <cstdlib>
#include "common.cpp"
using namespace std;

int main() {

    // INFO REQUESTS
    // REQUEST_METHOD tells us if browser did GET or POST
    // QUERY_STRING holds stuff after "?" in the URL (like action=bid)
    const char* requestMethod = getenv("REQUEST_METHOD");
    const char* queryString = getenv("QUERY_STRING");
    
    // 1. connect to the database. If fails, can't do anything else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // Minimal error page so the user sees something.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // 2. figure out session situation at the very start
    int currentUserId = 0;
    int sessionState = getSessionState(conn, currentUserId);
    
    // TRADE PAGE REQUIRES LOGIN:
    // user must be logged in to buy or sell
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
    
    // if logged in and active keep session alive by updating last_activity.
    string sessionId = getCookie("session");
    renewSessionActivity(conn, sessionId);
    
    // 3. prepare little log-in status message shown at top-left of page
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // 4. DETERMINE REQUEST METHOD AND GET DATA:
    // figure out if this is a GET request (viewing forms) or POST request (submitting forms)
    // GET data comes from the URL after the"?", POST data comes from form submission
    string method = (requestMethod != NULL) ? string(requestMethod) : "GET";
    
    string requestData = "";
    if (method == "GET") {
        // GET: read from the QUERY_STRING environment variable
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
    
    // pull form fields out of the request data
    string action = getValue(requestData, "action");
    
    // 5. HANDLE BID SUBMISSION:
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
        
		// an error page to show an auction wasn't found
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
        
        // BID ACCEPTED - RECORD IT IN DATABASE:
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
        
    // 6. HANDLE SELL SUBMISSION:
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
        
        // sql injection prevention:
        // escape item description before using in SQL
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
            cout << "<div class='error'>Failed to create auction.</div>";
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
        
    // 7. SHOW BID FORM FOR SPECIFIC AUCTION:
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
        
		// this error page is for: "Auction not found or has ended
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
        
        // can't let users bid on their own items
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
        
        // start sending our HTML page back to browser
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "<head>\n";
        cout << "    <meta charset=\"UTF-8\">\n";
        cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
        cout << "    <title>All-In Dragons Auctions - Place Bid</title>\n";
        cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
        cout << "</head>\n";
        cout << "<body>\n";
        
        // little status message in top-left that tells user their login status
        cout << "    <div class='status'><em>" << htmlEscape(statusMessage) << "</em></div>\n";
        
        // main content container: just a white box with stuff in it
        cout << "    <div class='container'>\n";
        cout << "        <h1>Place Your Bid</h1>\n";
        
        // NAVIGATION MENU:
        // links to other pages in the auction system
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings (Coming Soon)</a>\n";
        cout << "            <a href='./trade.cgi'>Buy/Sell</a>\n";
        cout << "            <a href='./transactions.cgi'>My Transactions</a>\n";
        cout << "            <a href='./login.cgi?action=logout'>Logout</a>\n";
        cout << "        </div>\n";
        
        // show auction details
        cout << "        <div class='auction-item'>\n";
        cout << "            <h3>Item: " << htmlEscape(itemDesc) << "</h3>\n";
        cout << "            <p><strong>Current Bid:</strong> $" << htmlEscape(currentBid) << "</p>\n";
        cout << "        </div>\n";
        
        // bid form: where users enter their bid amount
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
        
        // close up main container and page
        cout << "    </div>\n";
        cout << "</body>\n";
        cout << "</html>\n";
        
    // 8. DEFAULT VIEW - SHOW THE BUY/SELL PAGE:
    // if user makes no specific actions, show this main trade page with both sell form and buy dropdown
    } else {
        // start sending our HTML page back to browser
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";
        cout << "<html lang=\"en\">\n";
        cout << "<head>\n";
        cout << "    <meta charset='UTF-8'>\n";
        cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
        cout << "    <title>All-In Dragons Auctions - Buy/Sell</title>\n";
        cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
        cout << "</head>\n";
        cout << "<body>\n";
        
        // little status message in top-left that tells user their login status
        cout << "    <div class='status'><em>" << htmlEscape(statusMessage) << "</em></div>\n";
        
        // main content container: just a white box with stuff in it
        cout << "    <div class='container'>\n";
        cout << "        <h1>Buy or Sell Items</h1>\n";
        
        // NAVIGATION MENU:
        // links to other pages in the auction system
        cout << "        <div class='nav'>\n";
        cout << "            <a href='./listings.cgi'>Listings (Coming Soon)</a>\n";
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
                    
                    // abbreviate long descriptions so dropdown doesn't look bad
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
        
        // close up main container and page
        cout << "    </div>\n";
        cout << "</body>\n";
        cout << "</html>\n";
    }
    
    // closing database at the end 
    mysql_close(conn);
    return 0;
}