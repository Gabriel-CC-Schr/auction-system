//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: listings.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

// listings.cpp - Display all auction listings
// This file shows all open auctions, ordered by ending soonest

#include <iostream>
#include <string>
#include <ctime>
#include <mysql/mysql.h>
#include "common.cpp"
using namespace std;

int main() {

    // grab query strings from stuff in URL after the "?"
    // need this to show error or success messages
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // 1. connect to database: if fails can't check sessions or do much else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // We'll print a basic error page so the user sees something helpful.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // 2. figure out the session state (logged in, expired, or none)
    int currentUserId = 0;
    int sessionState = getSessionState(conn, currentUserId);
    
    // If logged in and active keep session alive by updating last_activity.
    if (sessionState == SESSION_LOGGED_IN) {
        string sessionId = getCookie("session");
        renewSessionActivity(conn, sessionId);
    }
    
    // if session expired, clear cookie so browser doesn't think it's still valid
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // 3. prepare little status message shown at top-left of page
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }
    
    // 4. UPDATE EXPIRED AUCTIONS BEFORE DISPLAYING:
    // before showing listings, mark any auctions that passed their end time as closed
    // this keeps the database clean and ensures we only show active auctions
    string updateQuery = "UPDATE auctions SET is_closed = TRUE WHERE end_time < NOW() AND is_closed = FALSE";
    mysql_query(conn, updateQuery.c_str());
    
    // 5. QUERY DATABASE FOR ALL OPEN AUCTIONS:
    // get all auctions that aren't closed yet, ordered by end time (soonest first)
    // this way users see auctions that are ending soon at the top of the list
    string listQuery = "SELECT auction_id, seller_id, item_description, starting_bid, "
                      "current_bid, start_time, end_time, winner_id "
                      "FROM auctions WHERE is_closed = FALSE "
                      "ORDER BY end_time ASC";
    
    if (mysql_query(conn, listQuery.c_str())) {
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Error</h1>";
        cout << "<p>Failed to fetch auctions.</p></body></html>\n";
        mysql_close(conn);
        return 1;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    
    // 6. start sending our HTML page back to browser
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>All-In Dragons Auctions - Listings</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    
    // little status message in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>Current Auctions</h1>\n";
    
    // NAVIGATION MENU:
    // links to other pages in the auction system
    cout << "      <div class=\"nav\">\n";
    cout << "        <a href=\"./listings.cgi\">Listings</a>\n";
    
    if (currentUserId > 0) {
        // if user already logged in, show links to other auction pages
        cout << "        <a href=\"./trade.cgi\">Buy/Sell</a>\n";
        cout << "        <a href=\"./transactions.cgi\">My Transactions</a>\n";
        cout << "        <a href=\"./login.cgi?action=logout\">Logout</a>\n";
    } else {
        // not logged in: show login link
        cout << "        <a href=\"./index.cgi\">Login/Register</a>\n";
    }
    
    cout << "      </div>\n";
    
    // 7. DISPLAY ALL ACTIVE AUCTIONS IN A TABLE:
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
            
            // calculate how much time is left using helper function
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
    
    // close up main container and page
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    // closing database at the end 
    mysql_free_result(result);
    mysql_close(conn);
    return 0;
}