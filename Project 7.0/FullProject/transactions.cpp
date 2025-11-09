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
#include "common.cpp"

using namespace std;

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
    
    // little status message in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "        <h1>My Transactions</h1>\n";
    
    // NAVIGATION MENU:
    // links to other pages in the auction system
    cout << "        <div class=\"nav\">\n";
    cout << "            <a href=\"./listings.cgi\">Listings (Coming Soon)</a>\n";
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