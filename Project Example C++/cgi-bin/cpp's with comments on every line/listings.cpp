// listings.cpp — Show all open auctions ordered by earliest ending (every line commented)

#include <bits/stdc++.h>                    // Include standard C++ library headers (strings, streams, containers, etc.)
#include <mariadb/mysql.h>                  // Include MariaDB/MySQL C client API for database connectivity
using namespace std;                        // Use the standard namespace to avoid repetitive std:: qualifiers

#define DB_HOST "localhost"                 // Database host (use localhost to match typical XAMPP grants)
#define DB_USER "root"                      // Database username (replace with a dedicated user in production)
#define DB_PASS ""                          // Database password (empty for default XAMPP root; set a real one in production)
#define DB_NAME "auctiondb"                 // Target database name created by your DDL
#define DB_PORT 3306                        // Default TCP port for MySQL/MariaDB

// ---------- Utility: minimal HTML escaping ----------
static string html_escape(const string& s){ // Return an HTML-escaped version of s to prevent HTML injection
    string o; o.reserve(s.size());          // Pre-allocate output buffer to reduce reallocations
    for(char c : s){                        // Iterate over each character in the input
        switch(c){                          // Handle special characters that need escaping
            case '&': o += "&amp;";  break; // Replace '&' with entity
            case '<': o += "&lt;";   break; // Replace '<' with entity
            case '>': o += "&gt;";   break; // Replace '>' with entity
            case '"': o += "&quot;"; break; // Replace '"' with entity
            case '\'':o += "&#39;";  break; // Replace '\'' with entity
            default:   o.push_back(c);      // Copy all other characters unchanged
        }                                   // End switch
    }                                       // End for
    return o;                               // Return the escaped result
}                                           // End html_escape

// ---------- CGI framing helpers ----------
static void header(){                                                   // Emit standard CGI HTTP response headers
    cout << "Content-Type: text/html\r\n\r\n";                          // Output Content-Type header and terminate headers with a blank line
}                                                                       // End header

static void top(){                                                      // Render the HTML document head and page header/navigation
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>Open Auctions</title>" // Start HTML and set title
         << "<style>"                                                   // Open a style block with minimal CSS
         << "body{font-family:sans-serif;max-width:1100px;margin:24px auto;padding:0 12px}" // Page layout and font
         << ".card{border:1px solid #ddd;border-radius:12px;padding:12px;margin:12px 0}"    // Card styling for each auction
         << ".grid{display:grid;grid-template-columns:2fr 1fr 1fr;gap:8px}"                 // Grid layout for card contents
         << "button,a.button{border:1px solid #ccc;border-radius:10px;padding:8px 12px;display:inline-block;text-decoration:none}" // Button/link styling
         << "small{color:#666}"                                         // Subtle text color for metadata
         << "</style></head><body>"                                     // Close head and open body
         << "<h1>Open Auctions</h1>"                                    // Page heading
         << "<p><a href='/cgi-bin/auth.cgi'>Login/Register</a> · "      // Link to authentication page
         << "<a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · "         // Link to bid/sell page
         << "<a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>"; // Link to transactions with a separator line
}                                                                       // End top

static void bottom(){                                                   // Close the HTML document
    cout << "</body></html>";                                           // Emit closing tags for body and html
}                                                                       // End bottom

// ---------- Database connection (ignore option files; do not require TLS) ----------
static MYSQL* db(){                                                     // Create and return a connected MYSQL* or nullptr on failure
    MYSQL* c = mysql_init(nullptr);                                     // Initialize a new MYSQL client handle
    mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4");                // Request utf8mb4 character set for proper Unicode handling
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF)        // If MariaDB Connector/C provides option to ignore config files
    { unsigned int nocnf=1; mysql_options(c, MARIADB_OPT_NOCONF, &nocnf); } // Ignore my.ini/option files to avoid hidden SSL enforcement
#endif                                                                  // End conditional for MARIADB_OPT_NOCONF
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)   // If client library supports SSL enforcement toggle
    { unsigned int enforce=0; mysql_options(c, MARIADB_OPT_SSL_ENFORCE, &enforce); } // Do NOT enforce TLS on the client
#endif                                                                  // End conditional for MARIADB_OPT_SSL_ENFORCE
#if defined(MYSQL_OPT_SSL_MODE)                                         // If linked against libmysql that supports SSL modes
#ifndef SSL_MODE_DISABLED                                               // Ensure SSL_MODE_DISABLED exists even if header lacks it
#define SSL_MODE_DISABLED 0                                             // Define constant value for disabled mode
#endif                                                                  // End define guard
    { int mode=SSL_MODE_DISABLED; mysql_options(c, MYSQL_OPT_SSL_MODE, &mode); } // Explicitly disable TLS to avoid 2026 errors
#endif                                                                  // End conditional for MYSQL_OPT_SSL_MODE
    if(!mysql_real_connect(c, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr; // Attempt to connect; return nullptr if it fails
    return c;                                                           // Return connected handle on success
}                                                                       // End db

// ---------- SQL escaping helper ----------
static string esc(MYSQL* c, const string& in){                          // Escape arbitrary text for safe SQL literal usage
    string out; out.resize(in.size()*2+1);                              // Reserve worst-case buffer size (every char escaped)
    unsigned long n = mysql_real_escape_string(c, out.data(), in.c_str(), in.size()); // Perform the escaping using client API
    out.resize(n);                                                      // Shrink buffer to actual escaped length
    return out;                                                         // Return the escaped string
}                                                                       // End esc

// ---------- Session: resolve current user from SESSION_ID cookie ----------
static long current_user(MYSQL* c){                                     // Return user_id for current session, or -1 if not logged in
    const char* ck = getenv("HTTP_COOKIE");                             // Read raw Cookie header from CGI environment
    if(!ck) return -1;                                                  // If there is no Cookie header, user is not authenticated
    string cookies = ck;                                                // Copy to std::string for easier manipulation
    string token;                                                       // Will hold SESSION_ID value
    string key = "SESSION_ID=";                                         // Cookie name to search for
    size_t p = cookies.find(key);                                       // Locate the start of the SESSION_ID cookie
    if(p!=string::npos){                                                // If found, extract its value up to next ';'
        size_t s = p + key.size();                                      // Compute start index of the value
        size_t e = cookies.find(';', s);                                // Find end index (semicolon) or string end
        token = cookies.substr(s, (e==string::npos?cookies.size():e) - s); // Extract SESSION_ID value substring
    }                                                                   // End if
    if(token.empty()) return -1;                                        // If token wasn’t present, consider user unauthenticated
    string t = esc(c, token);                                           // Escape token for safe SQL usage
    string q = "SELECT user_id FROM sessions WHERE session_id='"+t+"' AND expires_at>NOW() LIMIT 1"; // Build query to validate session
    if(mysql_query(c, q.c_str())!=0) return -1;                         // Execute query; on failure treat as not logged in
    MYSQL_RES* r = mysql_store_result(c);                               // Retrieve result set from server
    if(!r || mysql_num_rows(r)==0){                                     // If no matching row, session invalid or expired
        if(r) mysql_free_result(r);                                     // Free result set if it exists
        return -1;                                                      // Return unauthenticated
    }                                                                   // End if
    MYSQL_ROW row = mysql_fetch_row(r);                                 // Fetch the first row
    long uid = atol(row[0]);                                            // Convert user_id text to integer
    mysql_free_result(r);                                               // Free server-side result resources
    return uid;                                                         // Return authenticated user id
}                                                                       // End current_user

// ---------- CGI entry point ----------
int main(){                                                             // Program entry point called by web server (CGI)
    header();                                                           // Emit HTTP response headers
    top();                                                              // Render page header and navigation links
    MYSQL* c = db();                                                    // Establish database connection
    if(!c){                                                             // If connection failed
        cout << "<p style='color:#b00'>DB connection failed.</p>";      // Inform the user of the database error
        bottom();                                                       // Close HTML
        return 0;                                                       // Exit gracefully
    }                                                                   // End DB failure branch

    long uid = current_user(c);                                         // Determine whether a user is logged in and get their user_id

    string q =                                                          // Build SQL to fetch all open auctions with current price and seller
      "SELECT a.auction_id, i.title, i.description, "                   // Select auction id, item title, and description
      "FORMAT(i.starting_price,2) AS startp, "                          // Include formatted starting price
      "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS currentp, " // Compute/display current highest (or starting)
      "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s') AS ends_at, "        // Include human-readable end time
      "i.seller_id "                                                    // Include seller id to suppress Bid button for own items
      "FROM auctions a JOIN items i ON i.item_id=a.item_id "            // Join auctions with items for details
      "WHERE a.end_time>NOW() AND a.closed=0 "                          // Only auctions that are still open
      "ORDER BY a.end_time ASC";                                        // Sort by soonest ending first

    if(mysql_query(c, q.c_str())!=0){                                   // Execute the query; if it fails
        cout << "<p>Query failed.</p>";                                  // Show a generic failure message
        mysql_close(c);                                                  // Close the database handle
        bottom();                                                        // Close HTML
        return 0;                                                        // Exit
    }                                                                   // End query failure branch

    MYSQL_RES* r = mysql_store_result(c);                                // Retrieve the result set for iteration
    MYSQL_ROW row;                                                       // Row container for fetched records
    bool any=false;                                                      // Track whether we printed at least one auction

    while((row=mysql_fetch_row(r))){                                     // Loop through each auction row returned
        any=true;                                                        // Mark that we have at least one auction to show
        string aid     = row[0]?row[0]:"";                               // Auction id (string)
        string title   = row[1]?row[1]:"";                               // Item title
        string descr   = row[2]?row[2]:"";                               // Item description
        string startp  = row[3]?row[3]:"0.00";                           // Formatted starting price
        string currentp= row[4]?row[4]:"0.00";                           // Formatted current highest price
        string ends    = row[5]?row[5]:"";                               // Formatted end time
        long   seller  = row[6]?atol(row[6]):0;                          // Seller id (numeric) for logic below

        cout << "<div class='card'><div class='grid'>"                   // Start a card with grid layout for display
             << "<div><h3>" << html_escape(title) << "</h3>"             // Display escaped item title as a heading
             << "<p>" << html_escape(descr) << "</p>"                    // Display escaped item description
             << "<small>Ends: " << ends << "</small></div>"              // Show auction end time
             << "<div><strong>Starting</strong><br>$" << startp << "</div>" // Show starting price column
             << "<div><strong>Current</strong><br>$" << currentp << "</div>" // Show current price column
             << "</div>";                                                // Close grid container inside the card

        if(uid>0 && uid != seller){                                      // If a user is logged in and is not the seller
            cout << "<p><a class='button' href='/cgi-bin/bid_sell.cgi?mode=bid&auction_id=" // Start Bid link
                 << aid << "'>Bid</a></p>";                               // Add query param for auction_id and close the link
        } else if(uid==seller){                                          // If logged-in user is the seller
            cout << "<p><small>You are the seller.</small></p>";         // Inform them they cannot bid on their own item
        } else {                                                         // If not logged in at all
            cout << "<p><small><a class='button' href='/cgi-bin/auth.cgi'>Log in to bid</a></small></p>"; // Suggest logging in
        }                                                                // End conditional on login/seller

        cout << "</div>";                                                // Close the card wrapper div
    }                                                                    // End while loop over auctions

    if(!any) cout << "<p><em>No open auctions right now.</em></p>";      // If there were no open auctions, show an empty-state message

    mysql_free_result(r);                                                // Free the result set resources on the server
    mysql_close(c);                                                      // Close the database connection cleanly
    bottom();                                                            // Emit closing HTML tags
    return 0;                                                            // Return success to the web server
}