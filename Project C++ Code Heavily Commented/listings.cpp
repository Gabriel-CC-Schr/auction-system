// Include all standard C++ libraries at once (GCC shortcut)
// Provides: iostream for input/output, string for text, vector for arrays, etc.
#include <bits/stdc++.h>

// Include MySQL C library for database connectivity
// This allows us to query the database for auction information
#include <mysql/mysql.h>

// Use standard namespace so we don't need "std::" prefix everywhere
// Example: We can write "string" instead of "std::string"
using namespace std;

// Database connection constants - never change during program execution
// These tell us where and how to connect to the MySQL database
const string DB_HOST = "localhost";                      // Database is on same computer
const string DB_USER = "allindragons";   				 // MySQL username
const string DB_PASS = "snogardnilla_002";               // MySQL password
const string DB_NAME = "cs370_section2_allindragons";   // Name of database to use

/**
 * FUNCTION: send_html_header
 * PURPOSE: Send HTTP header telling browser we're sending HTML content
 * WHY: Every web response needs headers before actual content
 */
void send_html_header()
{
    // Content-Type header specifies what kind of data follows
    // text/html means we're sending a web page
    // \r\n\r\n means two line breaks - separates headers from body
    cout << "Content-Type: text/html\r\n\r\n";
}

/**
 * FUNCTION: get_session_token_from_cookie
 * PURPOSE: Extract session token from the HTTP cookie header
 * RETURNS: Session token string, or empty string if no session found
 * WHY: We need session token to check if user is logged in
 */
string get_session_token_from_cookie()
{
    // Try to get HTTP_COOKIE environment variable
    // This contains all cookies sent by the browser
    const char* cookie_header = getenv("HTTP_COOKIE");
    
    // If no cookies were sent, return empty string
    if (!cookie_header) return "";
    
    // Convert C-style string to C++ string for easier manipulation
    string cookies(cookie_header);
    
    // Search for "session=" in the cookie string
    // Example: "session=abc123; other_cookie=xyz"
    size_t session_pos = cookies.find("session=");
    
    // If "session=" not found, return empty string
    if (session_pos == string::npos) return "";
    
    // Calculate position where session value starts
    // "session=" is 8 characters, so add 8 to skip past it
    size_t start_pos = session_pos + 8;
    
    // Find the semicolon that marks end of this cookie value
    size_t end_pos = cookies.find(';', start_pos);
    
    // If no semicolon found, session goes to end of string
    if (end_pos == string::npos) end_pos = cookies.length();
    
    // Extract and return just the session token value
    return cookies.substr(start_pos, end_pos - start_pos);
}

/**
 * FUNCTION: get_user_from_session
 * PURPOSE: Validate session token and get user ID if valid
 * RETURNS: true if valid session, false if expired or invalid
 * WHY: Check if user is logged in and who they are
 */
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get session token from browser's cookies
    string session_token = get_session_token_from_cookie();
    
    // If no session token found, user is not logged in
    if (session_token.empty()) return false;
    
    // SQL query to find session that:
    // 1. Matches the token (id = ?)
    // 2. Is not expired (last_active within last 5 minutes)
    // DATE_SUB(NOW(), INTERVAL 5 MINUTE) = current time minus 5 minutes
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Initialize a prepared statement (prevents SQL injection)
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare (compile) the SQL query
    // If preparation fails, clean up and return false
    if (mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql)))
    {
        // Close statement to free memory
        mysql_stmt_close(session_statement);
        // Return false indicating validation failed
        return false;
    }
    
    // Create parameter binding structure for the session token
    MYSQL_BIND param_binding{};
    
    // Specify parameter is a text string
    param_binding.buffer_type = MYSQL_TYPE_STRING;
    
    // Point to our session token string
    param_binding.buffer = (void*)session_token.c_str();
    
    // Specify length of the token string
    param_binding.buffer_length = session_token.size();
    
    // Bind parameter to statement (replace ? with token value)
    mysql_stmt_bind_param(session_statement, &param_binding);
    
    // Execute the query
    mysql_stmt_execute(session_statement);
    
    // Create result binding structure to receive user_id
    MYSQL_BIND result_binding{};
    
    // Specify we expect a large integer (BIGINT)
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
    
    // If session is valid, update last_active timestamp
    if (session_valid)
    {
        // SQL to set last_active to current time
        // This keeps active users logged in
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        
        // Create new prepared statement for update
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        
        // Prepare the update statement
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        
        // Bind session token parameter (reuse param_binding)
        mysql_stmt_bind_param(update_stmt, &param_binding);
        
        // Execute the update
        mysql_stmt_execute(update_stmt);
        
        // Close update statement
        mysql_stmt_close(update_stmt);
    }
    
    // Return whether session was valid
    return session_valid;
}

/**
 * FUNCTION: get_current_user_id
 * PURPOSE: Convenience function to get logged-in user's ID
 * RETURNS: User ID if logged in, 0 if not logged in
 * WHY: Many functions need to know who's logged in - this simplifies it
 */
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    // Variable to store user ID
    unsigned long long user_id = 0;
    
    // Try to get user from session
    // If successful, user_id will be filled in
    // If failed, user_id stays 0
    get_user_from_session(database, user_id);
    
    // Return the user ID (0 if not logged in)
    return user_id;
}

/**
 * STRUCT: DatabaseConnection
 * PURPOSE: Manage MySQL connection using RAII pattern
 * RAII = Resource Acquisition Is Initialization
 * WHY: Automatically connects when created, disconnects when destroyed
 */
struct DatabaseConnection
{
    // Pointer to MySQL connection structure
    // nullptr means no connection yet
    MYSQL* mysql_connection = nullptr;
    
    // CONSTRUCTOR: Runs automatically when object is created
    // Opens connection to MySQL database
    DatabaseConnection()
    {
        // Initialize MySQL connection structure
        mysql_connection = mysql_init(nullptr);
        
        // Connect to MySQL server with our credentials
        // Parameters: connection, host, user, password, database, port, socket, flags
        mysql_real_connect(
            mysql_connection,              // Connection object we initialized
            DB_HOST.c_str(),              // Server hostname
            DB_USER.c_str(),              // MySQL username
            DB_PASS.c_str(),              // MySQL password
            DB_NAME.c_str(),              // Database name to use
            0,                             // Port number (0 = default 3306)
            nullptr,                       // Unix socket (not used)
            0                              // Client flags (none)
        );
    }
    
    // DESTRUCTOR: Runs automatically when object is destroyed
    // Closes connection and frees memory
    ~DatabaseConnection()
    {
        // If we have an active connection
        if (mysql_connection != nullptr)
        {
            // Close it to free resources
            mysql_close(mysql_connection);
        }
    }
};

/**
 * FUNCTION: close_expired_auctions
 * PURPOSE: Mark any auctions past their end_time as CLOSED
 * WHY: Maintenance to keep auction statuses accurate
 */
void close_expired_auctions(DatabaseConnection& database)
{
    // SQL to update auction status to CLOSED if:
    // 1. Currently OPEN
    // 2. End time has passed (end_time <= NOW())
    const char* close_expired_sql = 
        "UPDATE auctions SET status='CLOSED' "
        "WHERE status='OPEN' AND end_time <= NOW()";
    
    // Execute the query directly (no parameters needed)
    mysql_query(database.mysql_connection, close_expired_sql);
}

/**
 * FUNCTION: main
 * PURPOSE: Main entry point for listings CGI program
 * DISPLAYS: HTML page showing all open auctions
 * WHY: Users need to see what items are available to bid on
 */
int main()
{
    // Turn off C/C++ I/O synchronization for faster output
    ios::sync_with_stdio(false);
    
    // Send HTTP header indicating HTML content
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Include CSS stylesheet for styling
    cout << "<link rel=\"stylesheet\" href=\"style.css\">";
    
    // Create database connection (automatically connects)
    DatabaseConnection database;
    
    // Get current logged-in user ID (0 if not logged in)
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Perform maintenance: close any expired auctions
    close_expired_auctions(database);
    
    // SQL query to get all open auctions with current prices
    // COALESCE returns first non-NULL value
    // If no bids exist, use start_price as current price
    // If bids exist, use MAX(bid amount) as current price
    const char* auction_list_sql = R"SQL(
        SELECT a.id, a.title, a.description, a.start_price, a.end_time, a.seller_id,
               COALESCE((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id = a.id), a.start_price) AS current_price
        FROM auctions a
        WHERE a.status = 'OPEN' AND a.end_time > NOW()
        ORDER BY a.end_time ASC
    )SQL";
    
    // Create prepared statement for the query
    MYSQL_STMT* auction_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare the SQL statement
    mysql_stmt_prepare(auction_statement, auction_list_sql, strlen(auction_list_sql));
    
    // Execute the query
    mysql_stmt_execute(auction_statement);
    
    // Variables to receive query results
    unsigned long long auction_id;                     // Unique ID of the auction
    unsigned long long seller_id;                      // ID of user who created auction
    char auction_title_buffer[121];                    // Item title (max 120 chars + null)
    char auction_description_buffer[4097];             // Item description (max 4096 + null)
    char end_time_buffer[20];                          // When auction ends
    unsigned long title_length;                        // Actual length of title string
    unsigned long description_length;                  // Actual length of description
    unsigned long end_time_length;                     // Actual length of end_time string
    double starting_price;                             // Minimum bid price
    double current_price;                              // Current highest bid (or start price)
    
    // Create array of result bindings - one for each column in SELECT
    MYSQL_BIND result_bindings[7]{};
    
    // COLUMN 0: auction ID
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;     // It's a big integer
    result_bindings[0].buffer = &auction_id;                  // Store in auction_id variable
    
    // COLUMN 1: auction title
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;       // It's a text string
    result_bindings[1].buffer = auction_title_buffer;         // Store in this buffer
    result_bindings[1].buffer_length = 120;                   // Max characters to read
    result_bindings[1].length = &title_length;                // Store actual length here
    
    // COLUMN 2: auction description
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;       // It's a text string
    result_bindings[2].buffer = auction_description_buffer;   // Store in this buffer
    result_bindings[2].buffer_length = 4096;                  // Max characters to read
    result_bindings[2].length = &description_length;          // Store actual length here
    
    // COLUMN 3: starting price
    result_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;       // It's a decimal number
    result_bindings[3].buffer = &starting_price;              // Store in starting_price variable
    
    // COLUMN 4: end time
    result_bindings[4].buffer_type = MYSQL_TYPE_STRING;       // It's a datetime string
    result_bindings[4].buffer = end_time_buffer;              // Store in this buffer
    result_bindings[4].buffer_length = 20;                    // Max characters to read
    result_bindings[4].length = &end_time_length;             // Store actual length here
    
    // COLUMN 5: seller ID
    result_bindings[5].buffer_type = MYSQL_TYPE_LONGLONG;     // It's a big integer
    result_bindings[5].buffer = &seller_id;                   // Store in seller_id variable
    
    // COLUMN 6: current price (calculated from bids or start_price)
    result_bindings[6].buffer_type = MYSQL_TYPE_DOUBLE;       // It's a decimal number
    result_bindings[6].buffer = &current_price;               // Store in current_price variable
    
    // Bind all result structures to the statement
    mysql_stmt_bind_result(auction_statement, result_bindings);
    
    // Start rendering HTML page
    cout << "<h2>Open Auctions</h2>";
    
    // Show instruction text
    cout << "<div class='small'>Ending soonest first.</div>";
    
    // Start card container and list
    cout << "<div class='card'><ul class='list'>";
    
    // Set number formatting to fixed decimal notation
    cout.setf(std::ios::fixed);
    
    // Show 2 decimal places for prices (e.g., $12.50)
    cout << setprecision(2);
    
    // Loop through each auction result
    // mysql_stmt_fetch returns 0 while there are more rows
    while (mysql_stmt_fetch(auction_statement) == 0)
    {
        // Convert C-style strings to C++ strings using actual lengths
        // This is safer than assuming null termination
        string auction_title(auction_title_buffer, title_length);
        string auction_description(auction_description_buffer, description_length);
        string end_time_string(end_time_buffer, end_time_length);
        
        // Start list item
        cout << "<li>";
        
        // Left side: auction information
        cout << "<span>";
        
        // Show title in bold
        cout << "<strong>" << auction_title << "</strong><br>";
        
        // Show description in small text
        cout << "<span class='small'>" << auction_description << "</span><br>";
        
        // Show when auction ends in small text
        cout << "<span class='small'>Ends " << end_time_string << "</span>";
        
        // Close left side span
        cout << "</span>";
        
        // Right side: price and bid link
        cout << "<span>";
        
        // Show current price with dollar sign
        cout << "$" << current_price;
        
        // If user is logged in (not 0) AND user is not the seller
        // then show bid link
        if (current_user_id != 0 && current_user_id != seller_id)
        {
            // Add link to bidding page
            cout << " <a href='trade.html'>Bid</a>";
        }
        
        // Close right side span
        cout << "</span>";
        
        // Close list item
        cout << "</li>";
    }
    
    // Close list and card
    cout << "</ul></div>";
    
    // Close prepared statement to free resources
    mysql_stmt_close(auction_statement);
    
    // Exit program successfully
    return 0;
}