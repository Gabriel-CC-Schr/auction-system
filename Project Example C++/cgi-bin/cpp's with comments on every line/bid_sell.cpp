// bid_sell.cpp — Bid on items and create auctions via CGI (every line commented)

#include <bits/stdc++.h>                 // Include standard C++ library headers (strings, streams, containers, etc.)
#include <mariadb/mysql.h>               // Include MariaDB/MySQL C client API for database access
using namespace std;                     // Use the standard namespace to avoid std:: prefixes

#define DB_HOST "localhost"              // Database host (localhost matches typical XAMPP grants)
#define DB_USER "root"                   // Database user (replace with a dedicated user in production)
#define DB_PASS ""                       // Database password (empty for default XAMPP root; set a real one in prod)
#define DB_NAME "auctiondb"              // Target database name created by your DDL
#define DB_PORT 3306                     // Default MySQL/MariaDB TCP port

// ---------- HTML escaping utility ----------
static string html_escape(const string& s){           // Return an HTML-escaped copy of string s
    string o; o.reserve(s.size());                    // Reserve capacity to reduce reallocations
    for(char c : s){                                  // Loop through every character
        switch(c){                                    // Decide how to escape each character
            case '&': o += "&amp;";  break;           // Escape '&' to &amp;
            case '<': o += "&lt;";   break;           // Escape '<' to &lt;
            case '>': o += "&gt;";   break;           // Escape '>' to &gt;
            case '"': o += "&quot;"; break;           // Escape '"' to &quot;
            case '\'':o += "&#39;";  break;           // Escape '\'' to &#39;
            default:   o.push_back(c);                // Keep other characters unchanged
        }                                             // End switch
    }                                                 // End for
    return o;                                         // Return the escaped string
}                                                     // End html_escape

// ---------- URL decoding + simple form parser (x-www-form-urlencoded) ----------
static string url_decode(const string& s){            // Decode %XX sequences and '+' into spaces
    string out; out.reserve(s.size());                // Reserve capacity for performance
    for(size_t i=0;i<s.size();++i){                   // Iterate characters by index
        if(s[i]=='+') out.push_back(' ');             // '+' encodes a literal space character
        else if(s[i]=='%' && i+2<s.size()){           // '%' introduces a two-digit hex byte
            int v=0; sscanf(s.substr(i+1,2).c_str(), "%x", &v); // Convert two hex digits to integer
            out.push_back(static_cast<char>(v));      // Append the decoded byte to output
            i+=2;                                     // Skip the two hex digits we just consumed
        } else out.push_back(s[i]);                   // Otherwise copy character as-is
    }                                                 // End for
    return out;                                       // Return decoded string
}                                                     // End url_decode

static map<string,string> parse_kv(const string& s){  // Parse "a=b&c=d" into a map<string,string>
    map<string,string> m;                             // Map to hold parsed key/value pairs
    size_t i=0;                                       // Cursor index into s
    while(i<s.size()){                                // Continue until the end of the string
        size_t e = s.find('=', i);                    // Find the '=' that separates key from value
        if(e==string::npos) break;                    // If not found, stop parsing
        size_t a = s.find('&', e+1);                  // Find the next '&' that ends the pair
        string k = url_decode(s.substr(i, e-i));      // URL-decode the key substring
        string v = url_decode(s.substr(e+1, (a==string::npos?s.size():a)-(e+1))); // URL-decode the value substring
        m[k]=v;                                       // Insert the pair into the map (overwrites duplicate keys)
        if(a==string::npos) break;                    // If there is no '&', we are finished
        i = a+1;                                      // Advance to the character after '&'
    }                                                 // End while
    return m;                                         // Return the parsed map
}                                                     // End parse_kv

// ---------- CGI framing helpers ----------
static void header(){                                 // Emit HTTP response headers for CGI
    cout << "Content-Type: text/html\r\n\r\n";        // Print Content-Type header and a blank line terminator
}                                                     // End header

static void top(){                                    // Render HTML <head> and page header
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>Bid / Sell</title>"  // Output doctype, head, and title
         << "<style>body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}" // Basic layout styles
         << "form{margin:18px 0;padding:12px;border:1px solid #ddd;border-radius:10px}"   // Form block styling
         << "input,select,button,textarea{padding:8px;margin:6px 0}"                      // Control padding/margins
         << "label{display:block;margin-top:8px}"                                         // Labels on their own lines
         << "table{border-collapse:collapse;width:100%}th,td{border:1px solid #ddd;padding:8px}" // Table styling
         << "</style></head><body><h1>Bid / Sell</h1>"                                    // Close head; open body; add H1
         << "<p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · "                 // Navigation link to listings
         << "<a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>";          // Navigation link to transactions
}                                                     // End top

static void bottom(){                                 // Close the HTML document
    cout << "</body></html>";                         // Emit closing tags for body and html
}                                                     // End bottom

// ---------- Database connection (ignores my.ini, does not require TLS) ----------
static MYSQL* db(){                                   // Establish a DB connection and return MYSQL* or nullptr
    MYSQL* c = mysql_init(nullptr);                   // Initialize a new MYSQL handle
    mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4"); // Ensure UTF-8 charset for connection
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF) // If MariaDB client supports ignoring option files
    { unsigned int nocnf=1; mysql_options(c, MARIADB_OPT_NOCONF, &nocnf); } // Ignore option files (e.g., hidden SSL settings)
#endif                                                // End conditional for MARIADB_OPT_NOCONF
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE) // If client supports SSL enforcement toggle
    { unsigned int enforce=0; mysql_options(c, MARIADB_OPT_SSL_ENFORCE, &enforce); } // Do NOT enforce TLS at the client
#endif                                                // End conditional for MARIADB_OPT_SSL_ENFORCE
#if defined(MYSQL_OPT_SSL_MODE)                       // If libmysql exposes MYSQL_OPT_SSL_MODE
#ifndef SSL_MODE_DISABLED                             // Define SSL_MODE_DISABLED if headers don’t provide it
#define SSL_MODE_DISABLED 0                           // Value 0 corresponds to disabled SSL mode
#endif                                                // End define guard for SSL_MODE_DISABLED
    { int mode=SSL_MODE_DISABLED; mysql_options(c, MYSQL_OPT_SSL_MODE, &mode); } // Explicitly disable SSL/TLS
#endif                                                // End conditional for MYSQL_OPT_SSL_MODE
    if(!mysql_real_connect(c, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr; // Connect or return nullptr
    return c;                                         // Return connected handle on success
}                                                     // End db

// ---------- SQL escaping helper ----------
static string esc(MYSQL* c, const string& in){        // Escape arbitrary text for safe inclusion in SQL literals
    string out; out.resize(in.size()*2+1);            // Allocate worst-case buffer (every char could be escaped)
    unsigned long n = mysql_real_escape_string(c, out.data(), in.c_str(), in.size()); // Perform escaping using client API
    out.resize(n);                                     // Shrink buffer to actual escaped length
    return out;                                        // Return the escaped string
}                                                     // End esc

// ---------- Session: resolve current user from SESSION_ID cookie ----------
static long current_user(MYSQL* c){                   // Return user_id for current session or -1 if unauthenticated
    const char* ck = getenv("HTTP_COOKIE");           // Read raw Cookie header from CGI environment
    if(!ck) return -1;                                // If no cookies, user is not logged in
    string cookies = ck;                              // Copy cookies into a std::string for easier parsing
    string token;                                     // Will hold the SESSION_ID value if found
    string key = "SESSION_ID=";                       // Cookie name we’re looking for
    size_t p = cookies.find(key);                     // Find the beginning of "SESSION_ID=" in the cookie string
    if(p!=string::npos){                              // If found, extract its value
        size_t s = p + key.size();                    // Compute start index of value
        size_t e = cookies.find(';', s);              // Find end at next ';' or string end
        token = cookies.substr(s, (e==string::npos?cookies.size():e) - s); // Extract substring containing token value
    }                                                 // End if
    if(token.empty()) return -1;                      // If missing/empty, treat as not logged in
    string t = esc(c, token);                         // Escape token for safe SQL usage
    string q = "SELECT user_id FROM sessions WHERE session_id='"+t+"' AND expires_at>NOW() LIMIT 1"; // Query to validate session
    if(mysql_query(c, q.c_str())!=0) return -1;       // If query failed, treat as unauthenticated
    MYSQL_RES* r = mysql_store_result(c);             // Retrieve result set from server
    if(!r || mysql_num_rows(r)==0){                   // If no rows, session is invalid or expired
        if(r) mysql_free_result(r);                   // Free result set if it exists
        return -1;                                    // Return unauthenticated state
    }                                                 // End if
    MYSQL_ROW row = mysql_fetch_row(r);               // Fetch the first/only row
    long uid = atol(row[0]);                          // Convert user_id text to integer
    mysql_free_result(r);                             // Free server-side result resources
    return uid;                                       // Return authenticated user id
}                                                     // End current_user

// ---------- UI: show Bid and Sell forms ----------
static void show_forms(MYSQL* c, long uid, const string& preselectAuctionId=""){ // Render both forms; may preselect an auction
    cout << "<h2>Bid on an Item</h2>";                  // Section heading for bidding
    cout << "<form method='POST'><input type='hidden' name='action' value='bid'>"; // Begin bid form with hidden action=bid
    cout << "<label>Item:</label><select name='auction_id' required>"; // Dropdown list of items the user can bid on

    string q =                                          // Build query to fetch open auctions not owned by the current user
      "SELECT a.auction_id, i.title, "
      "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS cur "
      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
      "WHERE a.end_time>NOW() AND a.closed=0 AND i.seller_id<>"+to_string(uid)+" "
      "ORDER BY a.end_time ASC";                        // Sort by soonest ending

    if(mysql_query(c, q.c_str())==0){                   // Execute the query to fetch options
        MYSQL_RES* r = mysql_store_result(c);           // Retrieve the result set
        MYSQL_ROW row;                                  // Declare a row holder
        while((row=mysql_fetch_row(r))){                // Iterate over each returned row
            string aid    = row[0]?row[0]:"";           // Auction id (string form)
            string title  = row[1]?row[1]:"";           // Item title
            string cur    = row[2]?row[2]:"0.00";       // Current highest price or starting price
            cout << "<option value='"<< aid <<"' "      // Start option tag with value attribute
                 << (preselectAuctionId==aid?"selected":"") // Add selected if this matches preselected id
                 << ">" << html_escape(title)           // Display escaped title to prevent HTML injection
                 << " — Current: $" << cur              // Show current price alongside title
                 << "</option>";                        // Close option tag
        }                                               // End while loop
        mysql_free_result(r);                           // Free the result set resources
    }                                                   // End if query succeeded

    cout << "</select>";                                // Close the <select> element
    cout << "<label>Your Maximum Bid (USD):</label>"    // Label for bid amount
         << "<input type='number' name='amount' min='0' step='0.01' required>"; // Numeric input for bid amount
    cout << "<button type='submit'>Place Bid</button></form>"; // Submit button for placing a bid

    cout << "<h2>Sell an Item</h2>";                    // Section heading for selling
    cout << "<form method='POST'><input type='hidden' name='action' value='sell'>" // Begin sell form with hidden action=sell
            "<label>Title</label><input type='text' name='title' maxlength='150' required>" // Item title input
            "<label>Description</label><textarea name='description' rows='4' required></textarea>" // Item description textarea
            "<label>Starting Price (USD)</label><input type='number' name='starting_price' min='0' step='0.01' required>" // Starting price field
            "<label>Start Date & Time (YYYY-MM-DD HH:MM:SS)</label><input type='text' name='start_time' placeholder='2025-09-04 12:00:00' required>" // Start time input
            "<p><em>All auctions last exactly 168 hours (7 days).</em></p>" // Informational note about duration
            "<button type='submit'>Create Auction</button></form>"; // Submit button to create the auction
}                                                     // End show_forms

// ---------- CGI entry point ----------
int main(){                                           // Main function executed by the web server (CGI)
    header();                                         // Emit CGI HTTP response headers
    top();                                            // Output the page header and navigation
    MYSQL* c = db();                                  // Open a database connection
    if(!c){                                           // Check if connection failed
        cout << "<p style='color:#b00'>DB connection failed.</p>"; // Show an error message
        bottom();                                      // Close the HTML document
        return 0;                                      // Exit gracefully
    }                                                  // End connection failure branch

    long uid = current_user(c);                        // Determine the current logged-in user id from cookie/session
    if(uid<0){                                         // If not logged in
        cout << "<p style='color:#b00'>You are not logged in. <a href='/cgi-bin/auth.cgi'>Log in</a></p>"; // Prompt to log in
        mysql_close(c);                                // Close DB connection
        bottom();                                      // Close HTML
        return 0;                                      // Exit
    }                                                  // End not-logged-in branch

    string method = getenv("REQUEST_METHOD")?getenv("REQUEST_METHOD"):"GET"; // Read HTTP method with fallback to GET
    if(method=="GET"){                                 // If this is a GET request
        string qs = getenv("QUERY_STRING")?getenv("QUERY_STRING"):""; // Read the raw query string
        auto kv = parse_kv(qs);                        // Parse query parameters into a map
        string pre = kv.count("auction_id")?kv["auction_id"]:""; // Extract optional auction_id to preselect
        show_forms(c, uid, pre);                       // Render the forms with optional preselection
        mysql_close(c);                                // Close DB connection
        bottom();                                      // Close HTML
        return 0;                                      // Exit
    }                                                  // End GET branch

    const char* cl = getenv("CONTENT_LENGTH");         // Read the POST body length from environment
    size_t n = cl?strtoul(cl,nullptr,10):0;            // Convert it to a size_t (0 if missing)
    string body; body.resize(n);                       // Allocate a buffer to hold the POST body
    if(n) fread(body.data(),1,n,stdin);                // Read exactly n bytes from stdin into the buffer
    auto kv = parse_kv(body);                          // Parse the POST form fields into a map
    string action = kv.count("action")?kv["action"]:""; // Determine requested action ("bid" or "sell")

    if(action=="bid"){                                 // Handle the "place bid" action
        string auction_id = kv["auction_id"];          // Retrieve selected auction id from form
        string amount     = kv["amount"];              // Retrieve user’s maximum bid amount from form
        if(auction_id.empty()||amount.empty()){        // Validate presence of required fields
            cout << "<p>Missing fields.</p>";          // Inform the user about missing data
            show_forms(c, uid);                        // Re-render the forms
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End validation branch

        string aid = esc(c, auction_id);               // Escape auction_id for safe SQL usage
        string q =                                      // Build query to validate auction state and compute current price
          "SELECT a.auction_id, i.item_id, i.seller_id, i.starting_price, "
          "GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price) AS cur, "
          "a.end_time, a.closed "
          "FROM auctions a JOIN items i ON i.item_id=a.item_id "
          "WHERE a.auction_id="+aid+" LIMIT 1";        // Filter by the selected auction id

        if(mysql_query(c, q.c_str())!=0){              // Execute the query; if it fails
            cout << "<p>Bad auction.</p>";             // Show a generic error
            show_forms(c, uid);                        // Re-render forms
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End query failure branch

        MYSQL_RES* r = mysql_store_result(c);          // Store the query result set
        if(!r || mysql_num_rows(r)==0){                // If no rows returned, the auction id is invalid
            cout << "<p>Invalid auction.</p>";         // Inform user about invalid auction
            if(r) mysql_free_result(r);                // Free result set if allocated
            show_forms(c, uid);                        // Re-render forms
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End invalid auction branch

        MYSQL_ROW row = mysql_fetch_row(r);            // Fetch the single row from the result
        long   seller   = atol(row[2]);                // Parse seller_id (column 2) as long
        double starting = atof(row[3]);                // Parse starting_price (column 3) as double
        double current  = atof(row[4]);                // Parse computed current price (column 4) as double
        int    closed   = atoi(row[6]);                // Parse closed flag (column 6) as int
        mysql_free_result(r);                          // Free the result set resources

        if(seller==uid){                               // Disallow bidding on your own item
            cout << "<p style='color:#b00'>You cannot bid on your own item.</p>"; // Warn the user
            show_forms(c, uid, auction_id);            // Re-render with the same auction preselected
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End seller==uid branch

        if(closed){                                    // Disallow bidding on closed auctions
            cout << "<p style='color:#b00'>Auction is closed.</p>"; // Inform the user
            show_forms(c, uid, auction_id);            // Re-render with preselection
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End closed branch

        double amt = atof(amount.c_str());             // Convert amount string to a double
        if(amt < starting || amt <= current){          // Validate against starting and current price
            cout << "<p style='color:#b00'>Your max bid must be ≥ starting price ($" // Begin error message
                 << starting << ") and > current highest ($" << current << ").</p>"; // Complete message
            show_forms(c, uid, auction_id);            // Re-render with same auction selected
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End amount validation

        string ins = "INSERT INTO bids(auction_id,bidder_id,amount) VALUES(" // Build INSERT statement for the bid
                      + aid + "," + to_string(uid) + "," + to_string(amt) + ")"; // Include auction_id, user id, and amount
        if(mysql_query(c, ins.c_str())!=0){            // Execute the INSERT; on failure
            cout << "<p style='color:#b00'>Failed to place bid.</p>"; // Inform the user of failure
        } else {                                       // Otherwise success
            cout << "<p>Bid placed successfully.</p>"; // Confirm success to the user
        }                                              // End insert result branch

        show_forms(c, uid, auction_id);                // Re-render forms, keeping the same auction selected
        mysql_close(c);                                // Close DB
        bottom();                                      // Close HTML
        return 0;                                      // Exit
    }                                                  // End action=="bid" branch

    else if(action=="sell"){                           // Handle the "sell new item" action
        string title        = kv["title"];             // Extract title from form
        string description  = kv["description"];       // Extract description from form
        string start_price  = kv["starting_price"];    // Extract starting_price from form
        string start_time   = kv["start_time"];        // Extract start_time (YYYY-MM-DD HH:MM:SS) from form
        if(title.empty()||description.empty()||start_price.empty()||start_time.empty()){ // Validate presence of all fields
            cout << "<p>Missing fields.</p>";          // Inform user of missing inputs
            show_forms(c, uid);                        // Re-render forms
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End validation branch

        string t  = esc(c, title);                     // Escape title for SQL
        string d  = esc(c, description);               // Escape description for SQL
        string sp = esc(c, start_price);               // Escape starting price literal
        string st = esc(c, start_time);                // Escape start time string

        string insItem = "INSERT INTO items(seller_id,title,description,starting_price) VALUES(" // Build INSERT for items
                          + to_string(uid) + ",'" + t + "','" + d + "'," + sp + ")"; // Insert seller id, title, description, price
        if(mysql_query(c, insItem.c_str())!=0){        // Execute item insert; if it fails
            cout << "<p style='color:#b00'>Failed to create item.</p>"; // Show error message
            show_forms(c, uid);                        // Re-render forms
            mysql_close(c);                            // Close DB
            bottom();                                  // Close HTML
            return 0;                                  // Exit
        }                                              // End item insert failure branch

        unsigned long long item_id = mysql_insert_id(c); // Retrieve auto-incremented item_id from last insert

        string insA = "INSERT INTO auctions(item_id,start_time,end_time) VALUES(" // Build INSERT for auctions
                       + to_string(item_id) + ", '" + st + "', DATE_ADD('" + st + "', INTERVAL 168 HOUR))"; // 7-day auction
        if(mysql_query(c, insA.c_str())!=0){           // Execute the auction insert; if it fails
            cout << "<p style='color:#b00'>Failed to create auction (check datetime format).</p>"; // Inform user
        } else {                                       // Otherwise, success
            cout << "<p>Auction created! It will end 7 days after <strong>" // Begin success message
                 << html_escape(start_time) << "</strong>.</p>";            // Show the provided start time
        }                                              // End auction insert result

        show_forms(c, uid);                             // Re-render forms (ready for another action)
        mysql_close(c);                                 // Close DB connection
        bottom();                                       // Close HTML
        return 0;                                       // Exit
    }                                                  // End action=="sell" branch

    else{                                              // Fallback for unknown actions
        cout << "<p>Unknown action.</p>";              // Inform user that action was unrecognized
        show_forms(c, uid);                             // Re-render the forms for guidance
        mysql_close(c);                                 // Close DB
        bottom();                                       // Close HTML
        return 0;                                       // Exit
    }                                                  // End else unknown action
}                                                      // End main