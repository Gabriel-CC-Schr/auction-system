// Include all standard C++ libraries using GCC convenience header
// Provides: string, iostream, iomanip, vector, map, etc.
#include <bits/stdc++.h>

// Include MySQL C API library for database operations
// Allows querying user's bid history and auction results
#include <mysql/mysql.h>

// Use standard namespace to avoid "std::" prefixes
using namespace std;

// Database connection constants - never change at runtime
const string DB_HOST = "localhost";                      // Database server location
const string DB_USER = "allindragons";   				// MySQL username
const string DB_PASS = "snogardnilla_002";               // MySQL password
const string DB_NAME = "cs370_section2_allindragons";   // Database name

/**
 * FUNCTION: send_html_header
 * PURPOSE: Send HTTP Content-Type header before HTML output
 * WHY: HTTP protocol requires headers before body content
 */
void send_html_header()
{
    // Send Content-Type header
    // text/html tells browser we're sending a web page
    // \r\n\r\n is double line break separating headers from body
    cout << "Content-Type: text/html\r\n\r\n";
}

/**
 * FUNCTION: get_session_token_from_cookie
 * PURPOSE: Extract session token from HTTP cookie string
 * RETURNS: Session token if found, empty string if not
 * WHY: Need token to validate user authentication
 */
string get_session_token_from_cookie()
{
    // Get HTTP_COOKIE environment variable
    // Contains all cookies browser sent with request
    const char* cookie_header = getenv("HTTP_COOKIE");
    
    // If no cookies sent, return empty string
    if (!cookie_header) return "";
    
    // Convert C-style string to C++ string
    string cookies(cookie_header);
    
    // Search for "session=" in cookie string
    // Example: "session=abc123xyz; other=value"
    size_t session_pos = cookies.find("session=");
    
    // If "session=" not found, no session cookie
    if (session_pos == string::npos) return "";
    
    // Calculate position where token value starts
    // "session=" is 8 characters long
    size_t start_pos = session_pos + 8;
    
    // Find semicolon that ends this cookie
    size_t end_pos = cookies.find(';', start_pos);
    
    // If no semicolon, token goes to end of string
    if (end_pos == string::npos) end_pos = cookies.length();
    
    // Extract and return the token portion
    return cookies.substr(start_pos, end_pos - start_pos);
}

/**
 * FUNCTION: get_user_from_session
 * PURPOSE: Validate session token and retrieve user ID
 * RETURNS: true if session valid, false if expired or invalid
 * WHY: Transactions page requires authentication
 */
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get session token from browser cookies
    string session_token = get_session_token_from_cookie();
    
    // If no token present, user not logged in
    if (session_token.empty()) return false;
    
    // SQL query to find valid session
    // Session must match token AND be active within last 5 minutes
    // DATE_SUB(NOW(), INTERVAL 5 MINUTE) = current time minus 5 minutes
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Create prepared statement to prevent SQL injection
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare (compile) the SQL query
    // If preparation fails, clean up and return false
    if (mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql)))
    {
        // Close statement to free memory
        mysql_stmt_close(session_statement);
        // Return false - validation failed
        return false;
    }
    
    // Create parameter binding for session token
    MYSQL_BIND param_binding{};
    
    // Specify parameter is a text string
    param_binding.buffer_type = MYSQL_TYPE_STRING;
    
    // Point to our session token string
    param_binding.buffer = (void*)session_token.c_str();
    
    // Specify length of token string
    param_binding.buffer_length = session_token.size();
    
    // Bind parameter to statement (replaces ? in SQL)
    mysql_stmt_bind_param(session_statement, &param_binding);
    
    // Execute the query
    mysql_stmt_execute(session_statement);
    
    // Create result binding for user_id
    MYSQL_BIND result_binding{};
    
    // Specify result is a large integer (BIGINT)
    result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    
    // Point to variable where result should be stored
    result_binding.buffer = &out_user_id;
    
    // Bind result structure to statement
    mysql_stmt_bind_result(session_statement, &result_binding);
    
    // Try to fetch a result row
    // Returns 0 if row found, non-zero if no rows
    bool session_valid = (mysql_stmt_fetch(session_statement) == 0);
    
    // Close statement to free resources
    mysql_stmt_close(session_statement);
    
    // If session is valid, update activity timestamp
    if (session_valid)
    {
        // SQL to update last_active to current time
        // Keeps active users logged in
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        
        // Create prepared statement for update
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        
        // Prepare update query
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        
        // Bind token parameter (reuse param_binding from before)
        mysql_stmt_bind_param(update_stmt, &param_binding);
        
        // Execute update
        mysql_stmt_execute(update_stmt);
        
        // Close update statement
        mysql_stmt_close(update_stmt);
    }
    
    // Return whether session was valid
    return session_valid;
}

/**
 * FUNCTION: get_current_user_id
 * PURPOSE: Convenience wrapper to get logged-in user's ID
 * RETURNS: User ID if logged in, 0 if not
 * WHY: Simplifies checking who's logged in
 */
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    // Variable to hold user ID
    unsigned long long user_id = 0;
    
    // Try to get user from session
    // If successful, user_id is filled in
    // If failed, user_id remains 0
    get_user_from_session(database, user_id);
    
    // Return user ID (0 means not logged in)
    return user_id;
}

/**
 * STRUCT: DatabaseConnection
 * PURPOSE: Manage MySQL connection using RAII pattern
 * RAII = Resource Acquisition Is Initialization
 * WHY: Automatic connection management - no manual cleanup needed
 */
struct DatabaseConnection
{
    // Pointer to MySQL connection structure
    // nullptr means no connection yet
    MYSQL* mysql_connection = nullptr;
    
    // CONSTRUCTOR - runs automatically when object created
    // Opens connection to MySQL database
    DatabaseConnection()
    {
        // Initialize MySQL connection structure
        mysql_connection = mysql_init(nullptr);
        
        // Establish connection to MySQL server
        // Parameters: connection, host, user, password, database, port, socket, flags
        mysql_real_connect(
            mysql_connection,              // Connection object
            DB_HOST.c_str(),              // Server hostname
            DB_USER.c_str(),              // Username
            DB_PASS.c_str(),              // Password
            DB_NAME.c_str(),              // Database name
            0,                             // Port (0 = default 3306)
            nullptr,                       // Unix socket (not used)
            0                              // Client flags (none)
        );
    }
    
    // DESTRUCTOR - runs automatically when object destroyed
    // Closes connection and frees memory
    ~DatabaseConnection()
    {
        // If connection exists
        if (mysql_connection != nullptr)
        {
            // Close it
            mysql_close(mysql_connection);
        }
    }
};

/**
 * FUNCTION: display_active_bids_section
 * PURPOSE: Show user's current bids on open auctions
 * WHY: Users need to see where they're bidding and if they've been outbid
 */
void display_active_bids_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    // Start card container with heading
    cout << "<div class='card'><h3>Active Bids</h3>";
    
    // SQL query to find user's active bids
    // Shows auctions where:
    // 1. Status is OPEN (auction hasn't ended)
    // 2. Auction hasn't ended yet (end_time > NOW())
    // 3. User has placed a bid (b.bidder_id = ?)
    // 4. We're showing user's highest bid (MAX of their bids)
    // Also gets current top bid to check if user is outbid
    const char* active_bids_sql = R"SQL(
        SELECT a.title,
               (SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id = a.id) AS current_top_bid,
               b.amount AS my_bid_amount
        FROM auctions a
        JOIN bids b ON b.auction_id = a.id
        WHERE a.status = 'OPEN' 
          AND a.end_time > NOW() 
          AND b.bidder_id = ?
          AND b.amount = (SELECT MAX(b3.amount) FROM bids b3 WHERE b3.auction_id = a.id AND b3.bidder_id = ?)
        ORDER BY a.end_time ASC
    )SQL";
    
    // Create prepared statement
    MYSQL_STMT* active_bids_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL query
    mysql_stmt_prepare(active_bids_statement, active_bids_sql, strlen(active_bids_sql));
    
    // Create parameter bindings (user_id used twice in query)
    MYSQL_BIND parameter_bindings[2]{};
    
    // First parameter: user_id for WHERE clause
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &current_user_id;
    
    // Second parameter: same user_id for subquery
    parameter_bindings[1] = parameter_bindings[0];
    
    // Bind parameters to statement
    mysql_stmt_bind_param(active_bids_statement, parameter_bindings);
    
    // Execute query
    mysql_stmt_execute(active_bids_statement);
    
    // Variables to receive query results
    char auction_title_buffer[121];        // Auction title
    unsigned long title_length;            // Actual title length
    double current_top_bid;                // Highest bid on auction
    double my_bid_amount;                  // User's bid amount
    
    // Create result bindings array (3 columns)
    MYSQL_BIND result_bindings[3]{};
    
    // Column 0: auction title
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Column 1: current top bid
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &current_top_bid;
    
    // Column 2: user's bid amount
    result_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[2].buffer = &my_bid_amount;
    
    // Bind result structures to statement
    mysql_stmt_bind_result(active_bids_statement, result_bindings);
    
    // Start unordered list for bids
    cout << "<ul class='list'>";
    
    // Set decimal formatting for currency
    // fixed = use fixed-point notation (not scientific)
    cout.setf(std::ios::fixed);
    
    // Show 2 decimal places (e.g., $12.50)
    cout << setprecision(2);
    
    // Loop through each active bid result
    // mysql_stmt_fetch returns 0 while more rows exist
    while (mysql_stmt_fetch(active_bids_statement) == 0)
    {
        // Convert title from C-style to C++ string
        // Use actual length for safety
        string auction_title(auction_title_buffer, title_length);
        
        // Check if user has been outbid
        // If their bid is less than current top, someone outbid them
        bool user_is_outbid = (my_bid_amount < current_top_bid);
        
        // Start list item
        cout << "<li>";
        
        // Left side: auction info
        cout << "<span>";
        
        // Show auction title in bold
        cout << "<strong>" << auction_title << "</strong>";
        
        // If user was outbid, show warning badge
        if (user_is_outbid)
        {
            cout << " <span class='notice'>Outbid</span>";
        }
        
        // Close left span
        cout << "</span>";
        
        // Right side: current price and action link
        cout << "<span>";
        
        // Show current top bid amount
        cout << "$" << current_top_bid << " <a href='trade.html'>Increase bid</a>";
        
        // Close right span
        cout << "</span>";
        
        // Close list item
        cout << "</li>";
    }
    
    // Close list
    cout << "</ul>";
    
    // Close statement to free resources
    mysql_stmt_close(active_bids_statement);
    
    // Close card div
    cout << "</div>";
}

/**
 * FUNCTION: display_lost_auctions_section
 * PURPOSE: Show auctions where user bid but didn't win
 * WHY: Users want to see auction outcomes and winning prices
 */
void display_lost_auctions_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    // Start card with heading
    cout << "<div class='card'><h3>Didn't Win</h3>";
    
    // SQL query to find lost auctions
    // Shows closed auctions where:
    // 1. User placed at least one bid (EXISTS check)
    // 2. User did NOT have winning bid (NOT EXISTS check)
    // Winning bid = highest bid amount for that auction
    const char* lost_auctions_sql = R"SQL(
        SELECT a.title,
               (SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id = a.id) AS winning_amount
        FROM auctions a
        WHERE a.status = 'CLOSED' 
          AND EXISTS (
              SELECT 1 FROM bids bx 
              WHERE bx.auction_id = a.id AND bx.bidder_id = ?
          )
          AND NOT EXISTS (
              SELECT 1 FROM bids bw 
              WHERE bw.auction_id = a.id 
                AND bw.bidder_id = ? 
                AND bw.amount = (
                    SELECT MAX(b3.amount) FROM bids b3 WHERE b3.auction_id = a.id
                )
          )
        ORDER BY a.end_time DESC
    )SQL";
    
    // Create prepared statement
    MYSQL_STMT* lost_auctions_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(lost_auctions_statement, lost_auctions_sql, strlen(lost_auctions_sql));
    
    // Create parameter bindings (user_id used twice)
    MYSQL_BIND parameter_bindings[2]{};
    
    // First parameter: check if user bid
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &current_user_id;
    
    // Second parameter: check if user won
    parameter_bindings[1] = parameter_bindings[0];
    
    // Bind parameters
    mysql_stmt_bind_param(lost_auctions_statement, parameter_bindings);
    
    // Execute query
    mysql_stmt_execute(lost_auctions_statement);
    
    // Variables to receive results
    char auction_title_buffer[121];        // Auction title
    unsigned long title_length;            // Actual title length
    double winning_amount;                 // Amount that won the auction
    
    // Create result bindings (2 columns)
    MYSQL_BIND result_bindings[2]{};
    
    // Column 0: auction title
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Column 1: winning bid amount
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &winning_amount;
    
    // Bind results
    mysql_stmt_bind_result(lost_auctions_statement, result_bindings);
    
    // Start list
    cout << "<ul class='list'>";
    
    // Set decimal formatting
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Loop through each lost auction
    while (mysql_stmt_fetch(lost_auctions_statement) == 0)
    {
        // Convert title to C++ string
        string auction_title(auction_title_buffer, title_length);
        
        // Display list item
        cout << "<li>";
        
        // Left side: auction title
        cout << "<span><strong>" << auction_title << "</strong></span>";
        
        // Right side: winning price
        cout << "<span>$" << winning_amount << "</span>";
        
        // Close list item
        cout << "</li>";
    }
    
    // Close list
    cout << "</ul>";
    
    // Close statement
    mysql_stmt_close(lost_auctions_statement);
    
    // Close card
    cout << "</div>";
}

/**
 * FUNCTION: main
 * PURPOSE: Main entry point for transactions CGI program
 * DISPLAYS: User's active bids and lost auction history
 * WHY: Users need to track their bidding activity
 */
int main()
{
    // Turn off C/C++ I/O synchronization for faster output
    // Performance optimization
    ios::sync_with_stdio(false);
    
    // Send HTTP header
    send_html_header();
    
    // Create database connection (auto-connects in constructor)
    DatabaseConnection database;
    
    // Get current logged-in user's ID
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Check if user is logged in
    if (current_user_id == 0)
    {
        // Not logged in - redirect to auth page
        // Status 303 = See Other (redirect)
        cout << "Status: 303 See Other\r\n"
             // Location header tells browser where to go
             << "Location: auth.html\r\n"
             // Content-Type for any fallback content
             << "Content-Type: text/html\r\n\r\n"
             // Message for browsers that don't auto-redirect
             << "Session expired. Please log in.";
        
        // Exit program
        return 0;
    }
    
    // User is logged in - show their transactions
    
    // Display section showing active bids
    // Shows which auctions user is currently bidding on
    // Indicates if they've been outbid
    display_active_bids_section(database, current_user_id);
    
    // Display section showing lost auctions
    // Shows closed auctions where user bid but didn't win
    // Shows the winning bid amount for reference
    display_lost_auctions_section(database, current_user_id);
    
    // Exit program successfully
    return 0;
}