// transactions.cpp — Display user transactions (Selling, Purchases, Current Bids, Didn't Win) — every line commented

#include <bits/stdc++.h>                     // Include standard C++ library headers (strings, i/o, containers, etc.)
#include <mariadb/mysql.h>                   // Include MariaDB/MySQL C client API for database connectivity
using namespace std;                         // Use the standard namespace to avoid std:: prefixes everywhere

#define DB_HOST "localhost"                  // Database host name (localhost to match your user grants)
#define DB_USER "root"                       // Database username (replace with a dedicated user in production)
#define DB_PASS ""                           // Database password (empty for default XAMPP root; set real one later)
#define DB_NAME "auctiondb"                  // Name of the database created by your DDL
#define DB_PORT 3306                         // TCP port for MySQL/MariaDB (3306 is the default)

// ---------- Minimal HTML escaping (defensive for any direct string output) ----------
static string html_escape(const string& s){  // Returns a copy of s with HTML special chars replaced by entities
    string o; o.reserve(s.size());           // Reserve capacity to reduce reallocations for speed
    for(char c : s){                         // Iterate over each character in the input
        switch(c){                           // Check which character we are looking at
            case '&': o += "&amp;";  break;  // Escape ampersand
            case '<': o += "&lt;";   break;  // Escape less-than
            case '>': o += "&gt;";   break;  // Escape greater-than
            case '"': o += "&quot;"; break;  // Escape double-quote
            case '\'':o += "&#39;";  break;  // Escape single-quote
            default:   o.push_back(c);       // Copy all other characters unchanged
        }                                    // End switch
    }                                        // End for
    return o;                                // Return the escaped string
}                                            // End html_escape

// ---------- CGI helpers for page framing ----------
static void header(){                        // Emit HTTP headers for CGI response
    cout << "Content-Type: text/html\r\n\r\n"; // Output MIME type, then blank line to end headers
}                                            // End header

static void top(){                           // Emit the opening HTML and page header with light styles
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>Your Transactions</title>" // Start HTML document and set title
         << "<style>"                        // Begin inline CSS block
         << "body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}"     // Body font and layout
         << "h2{margin-top:28px}"            // Spacing above section headers
         << "table{border-collapse:collapse;width:100%}"                                        // Table takes full width
         << "th,td{border:1px solid #ddd;padding:8px}"                                          // Cell borders and padding
         << ".warn{color:#b00;font-weight:bold}"                                                // Styling for warnings
         << "</style></head><body>"          // Close head and open body
         << "<h1>Your Transactions</h1>"     // Main page heading
         << "<p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · "                       // Link to listings page
         << "<a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a></p><hr>";                           // Link to bid/sell and a divider
}                                            // End top

static void bottom(){                        // Finish the HTML document
    cout << "</body></html>";                // Emit closing tags for body and html
}                                            // End bottom

// ---------- Database connection (ignore option files; disable TLS requirement to avoid 2026 errors) ----------
static MYSQL* db_connect(){                  // Create and return a connected MYSQL* or nullptr on failure
    MYSQL* conn = mysql_init(nullptr);       // Allocate and initialize a client handle
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4"); // Request UTF-8 charset for proper text handling
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF) // If using MariaDB client with no-config option
    { unsigned int nocnf = 1; mysql_options(conn, MARIADB_OPT_NOCONF, &nocnf); } // Ignore my.ini/option files (prevents hidden SSL flags)
#endif                                     // End conditional for MARIADB_OPT_NOCONF
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE) // If SSL enforcement toggle is available
    { unsigned int enforce = 0; mysql_options(conn, MARIADB_OPT_SSL_ENFORCE, &enforce); } // Do NOT enforce TLS from client side
#endif                                     // End conditional for MARIADB_OPT_SSL_ENFORCE
#if defined(MYSQL_OPT_SSL_MODE)             // If building against libmysql that supports SSL mode selection
#ifndef SSL_MODE_DISABLED                   // If the header doesn't define SSL_MODE_DISABLED, define it
#define SSL_MODE_DISABLED 0                 // 0 corresponds to "disabled" for SSL mode
#endif                                     // End define guard
    { int mode = SSL_MODE_DISABLED; mysql_options(conn, MYSQL_OPT_SSL_MODE, &mode); } // Explicitly disable SSL/TLS
#endif                                     // End conditional for MYSQL_OPT_SSL_MODE
    if(!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) // Attempt TCP connection to server
        return nullptr;                    // If connection failed, return nullptr to caller
    return conn;                           // On success, return the connected handle
}                                          // End db_connect

// ---------- SQL escaping helper ----------
static string esc(MYSQL* c, const string& in){ // Escape a string for safe use in SQL literal contexts
    string out; out.resize(in.size()*2+1);     // Allocate worst-case buffer (every char escaped)
    unsigned long n = mysql_real_escape_string(c, out.data(), in.c_str(), in.size()); // Perform escaping via client API
    out.resize(n);                              // Shrink to actual length after escaping
    return out;                                 // Return escaped string
}                                               // End esc

// ---------- Cookie helpers ----------
static string get_cookie_raw(){                 // Retrieve the raw Cookie header from CGI environment
    const char* ck = getenv("HTTP_COOKIE");     // Read HTTP_COOKIE environment variable set by web server
    return ck ? string(ck) : string();          // Wrap in std::string or return empty string if missing
}                                               // End get_cookie_raw

static string get_cookie_value(const string& name){ // Extract a specific cookie by name from the raw header
    string all = get_cookie_raw();              // Get the full Cookie header string
    if(all.empty()) return "";                  // If there are no cookies, return empty
    string key = name + "=";                    // Compose the search prefix like "SESSION_ID="
    size_t p = all.find(key);                   // Find the start of the named cookie
    if(p == string::npos) return "";            // If not found, return empty
    size_t s = p + key.size();                  // Compute the start index of the cookie value
    size_t e = all.find(';', s);                // Find the end of the cookie value at next ';' or end of string
    return all.substr(s, (e==string::npos? all.size():e) - s); // Return substring containing just the value
}                                               // End get_cookie_value

// ---------- Resolve the logged-in user from the sessions table ----------
static long current_user(MYSQL* c){             // Return user_id if a valid session exists, otherwise -1
    string token = get_cookie_value("SESSION_ID"); // Read SESSION_ID cookie value from request
    if(token.empty()) return -1;                // If no token present, user is not logged in
    string t = esc(c, token);                   // Escape the token for safe SQL usage
    string q = "SELECT user_id FROM sessions WHERE session_id='"+t+"' AND expires_at>NOW() LIMIT 1"; // Build query to validate session
    if(mysql_query(c, q.c_str())!=0) return -1; // If query execution fails, consider unauthenticated
    MYSQL_RES* r = mysql_store_result(c);       // Store the server result
    if(!r || mysql_num_rows(r)==0){             // If no rows returned, session invalid or expired
        if(r) mysql_free_result(r);             // Free result if it was allocated
        return -1;                              // Indicate not logged in
    }                                           // End session-not-found branch
    MYSQL_ROW row = mysql_fetch_row(r);         // Fetch first row of the result set
    long uid = row && row[0] ? atol(row[0]) : -1; // Convert user_id text to integer (or -1 on error)
    mysql_free_result(r);                       // Free server-side resources for the result
    return uid;                                 // Return the resolved user_id
}                                               // End current_user

// ---------- Generic helper: run a SELECT and render a simple HTML table ----------
static void run_and_print_table(                 // Render an HTML table for the provided SQL statement
    MYSQL* db,                                   // Active database connection
    const string& sql,                           // SQL query to execute
    const vector<string>& headers                // Column headers to display in the table
){
    if(mysql_query(db, sql.c_str())!=0){         // Execute the SQL query; check for failure
        cout << "<p class='warn'>Query failed.</p>"; // Show a warning if query failed
        return;                                  // Bail out of the function
    }                                            // End query error branch
    MYSQL_RES* res = mysql_store_result(db);     // Retrieve the entire result set from the server
    if(!res){                                    // If result retrieval failed
        cout << "<p class='warn'>No results.</p>"; // Inform the user that no results were produced
        return;                                  // Return early
    }                                            // End no-result branch
    if(mysql_num_rows(res)==0){                  // If there are zero rows in the result set
        cout << "<p><em>None.</em></p>";         // Display a friendly empty-state message
        mysql_free_result(res);                  // Free the result set resources
        return;                                  // Return early
    }                                            // End empty-set branch
    cout << "<table><thead><tr>";                // Begin rendering the table with header row
    for(const auto& h : headers)                 // Iterate each provided column header label
        cout << "<th>" << html_escape(h) << "</th>"; // Output header cell with safe text
    cout << "</tr></thead><tbody>";              // Close header row and open body section

    MYSQL_ROW row;                                // Declare a row pointer for iteration
    unsigned int ncols = mysql_num_fields(res);   // Determine how many columns are in each row
    while((row = mysql_fetch_row(res))){          // Fetch each row until none left
        cout << "<tr>";                           // Begin a new table row
        for(unsigned int i=0;i<ncols;i++){        // Loop through each column in the current row
            const char* v = row[i] ? row[i] : ""; // Read value or use empty string if NULL
            cout << "<td>" << v << "</td>";       // Emit raw value into cell (values are generated by us/DB)
        }                                         // End for columns
        cout << "</tr>";                          // End the current table row
    }                                            // End while over rows
    cout << "</tbody></table>";                  // Close the table body and the table
    mysql_free_result(res);                      // Free result set resources after rendering
}                                               // End run_and_print_table

// ---------- CGI entry point ----------
int main(){                                     // Program entry point, invoked by the web server as a CGI
    header();                                   // Emit HTTP headers (Content-Type)
    top();                                      // Emit page header and navigation
    MYSQL* db = db_connect();                   // Attempt to connect to the database server
    if(!db){                                    // Check if the database connection failed
        cout << "<p class='warn'>DB connection failed.</p>"; // Show an error message if DB unavailable
        bottom();                               // Close the HTML document
        return 0;                               // Exit gracefully
    }                                           // End DB-connection-failure branch

    long uid = current_user(db);                // Resolve the logged-in user_id from session cookie
    if(uid < 0){                                // If not logged in (no session or expired)
        cout << "<p class='warn'>You are not logged in. <a href='/cgi-bin/auth.cgi'>Log in</a></p>"; // Prompt to log in
        mysql_close(db);                        // Close the DB connection before exiting
        bottom();                               // Close the HTML document
        return 0;                               // Exit the CGI
    }                                           // End not-logged-in branch

    // ----- Section 1: Selling — Active (items you're selling that haven't ended) -----
    cout << "<h2>1. Selling — Active</h2>";     // Print section header
    {                                           // Open a local scope for variables
        vector<string> cols = {"Item","Starting Price","Ends","Current Highest"}; // Define table headers
        string q =                               // Build SQL to list active auctions for this seller
          "SELECT i.title, "                     // Item title
          "FORMAT(i.starting_price,2), "         // Starting price formatted to 2 decimals
          "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), " // Auction end time formatted nicely
          "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) " // Current highest or starting price
          "FROM items i JOIN auctions a ON a.item_id=i.item_id " // Join items and auctions
          "WHERE i.seller_id=" + to_string(uid) + " AND a.end_time>NOW() AND a.closed=0 " // Only your items, still open
          "ORDER BY a.end_time ASC";             // Soonest ending first
        run_and_print_table(db, q, cols);        // Execute and render table for this section
    }                                           // End local scope for Selling — Active

    // ----- Section 1b: Selling — Sold (your auctions that have ended) -----
    cout << "<h2>1. Selling — Sold</h2>";       // Print section header
    {                                           // Open local scope
        vector<string> cols = {"Item","Ended","Winning Bid"}; // Define columns for sold items
        string q =                               // Build SQL to list closed auctions for this seller
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), " // Item title and when it ended
          "FORMAT(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id), i.starting_price),2) " // Winning bid or starting price if no bids
          "FROM items i JOIN auctions a ON a.item_id=i.item_id " // Join items and auctions
          "WHERE i.seller_id=" + to_string(uid) + " AND a.end_time<=NOW() " // Only your items that have ended
          "ORDER BY a.end_time DESC";             // Most recently ended first
        run_and_print_table(db, q, cols);        // Execute and render the table
    }                                           // End local scope for Selling — Sold

    // ----- Section 2: Purchases (auctions you won) -----
    cout << "<h2>2. Purchases</h2>";            // Print section header
    {                                           // Open local scope
        vector<string> cols = {"Item","Ended","Your Winning Bid"}; // Columns for purchases
        string q =                               // Build SQL to list auctions where your bid was the highest at close
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) " // Title, end time, your winning amount
          "FROM auctions a "                     // Start from auctions
          "JOIN items i ON i.item_id=a.item_id " // Join items for titles
          "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id " // Subquery to compute max bid per auction
          "JOIN bids b ON b.auction_id=a.auction_id AND b.amount=m.max_amt " // Join back to the actual winning bid row
          "WHERE a.end_time<=NOW() AND b.bidder_id=" + to_string(uid) + " "  // Closed auctions where you are the winner
          "ORDER BY a.end_time DESC";             // Most recent purchases first
        run_and_print_table(db, q, cols);        // Execute and render the purchases table
    }                                           // End local scope for Purchases

    // ----- Section 3: Current Bids (open auctions you’re bidding on, status + action) -----
    cout << "<h2>3. Current Bids</h2><p>Click “Increase Max Bid” to raise your bid.</p>"; // Section header and hint
    {                                           // Open local scope
        vector<string> cols = {"Item","Ends","Your Max Bid","Current Highest","Status","Action"}; // Define columns
        string q =                               // Build SQL to show each auction where you’ve bid, and your status
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), " // Item title and end time
          "FORMAT(ub.max_amt,2), "               // Your maximum bid on that auction
          "FORMAT(GREATEST(IFNULL(allb.max_all,0), i.starting_price),2), " // Current highest overall (or starting)
          "CASE WHEN ub.max_amt >= GREATEST(IFNULL(allb.max_all,0), i.starting_price) " // Determine if you’re leading
          "     AND ub.max_amt = allb.max_all THEN 'Leading' ELSE 'Outbid' END, " // Label as Leading or Outbid
          "CONCAT('<a href=\"/cgi-bin/bid_sell.cgi?mode=bid&auction_id=', a.auction_id, '\">Increase Max Bid</a>') " // Action link to raise your bid
          "FROM auctions a "                     // Base table
          "JOIN items i ON i.item_id=a.item_id " // Join items for title
          "JOIN (SELECT auction_id, bidder_id, MAX(amount) AS max_amt FROM bids WHERE bidder_id=" + to_string(uid) + " GROUP BY auction_id, bidder_id) ub " // Your max per auction
          "  ON ub.auction_id=a.auction_id "     // Join your max to its auction
          "LEFT JOIN (SELECT auction_id, MAX(amount) AS max_all FROM bids GROUP BY auction_id) allb " // Everyone’s max per auction
          "  ON allb.auction_id=a.auction_id "   // Join overall max to auction
          "WHERE a.end_time>NOW() AND a.closed=0 " // Only auctions still open
          "ORDER BY a.end_time ASC";             // Soonest ending at the top
        run_and_print_table(db, q, cols);        // Execute and render the current bids table
        cout << "<p class='warn'>If “Status” says Outbid, your max bid is lower than someone else’s.</p>"; // Clarify the status meaning
    }                                           // End local scope for Current Bids

    // ----- Section 4: Didn’t Win (auctions you bid on but lost) -----
    cout << "<h2>4. Didn’t Win</h2>";           // Print section header
    {                                           // Open local scope
        vector<string> cols = {"Item","Ended","Winning Bid"}; // Columns for lost auctions
        string q =                               // Build SQL to show auctions you bid on but didn’t win
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) " // Title, end time, winning bid
          "FROM auctions a "                     // Start from auctions
          "JOIN items i ON i.item_id=a.item_id " // Join items for titles
          "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id " // Max bid per auction
          "LEFT JOIN (SELECT auction_id, bidder_id, MAX(amount) AS mymax FROM bids WHERE bidder_id=" + to_string(uid) + " GROUP BY auction_id, bidder_id) me " // Your max per auction
          "  ON me.auction_id=a.auction_id "     // Join your max to auction
          "WHERE a.end_time<=NOW() AND (me.mymax IS NULL OR me.mymax < m.max_amt) " // Closed auctions where you’re not the winner
          "ORDER BY a.end_time DESC";             // Most recent losses first
        run_and_print_table(db, q, cols);        // Execute and render the table of lost auctions
    }                                           // End local scope for Didn’t Win

    mysql_close(db);                            // Close the database connection cleanly
    bottom();                                   // Emit closing HTML markup
    return 0;                                   // Return success code to the web server
}                                              // End main