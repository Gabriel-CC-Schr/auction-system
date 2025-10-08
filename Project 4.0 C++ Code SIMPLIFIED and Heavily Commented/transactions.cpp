// *******
// transactions.cpp
// *******


// This header includes common C++ features like cout, string, vectors, etc.
#include <bits/stdc++.h>
// This header provides MySQL database functionality
#include <mysql/mysql.h>

// This lets us use standard library features without typing "std::" every time
using namespace std;

// Database connection constants - these tell us how to connect to MySQL
const string DB_HOST = "localhost";        // Server location (localhost = this computer)
const string DB_USER = "allindragons";     // Database username
const string DB_PASS = "snogardnilla_002"; // Database password
const string DB_NAME = "cs370_section2_allindragons"; // Which database to use

// This struct manages our database connection
// It automatically connects when created and disconnects when destroyed
struct DatabaseConnection
{
    // Pointer to hold the MySQL connection object
    MYSQL* mysql_connection;
    
    // Constructor - runs automatically when we create a DatabaseConnection
    DatabaseConnection()
    {
        // Initialize pointer to null first
        mysql_connection = nullptr;
        // Initialize the MySQL library
        mysql_connection = mysql_init(nullptr);
        
        // Actually connect to the database server
        MYSQL* conn = mysql_real_connect(
            mysql_connection,      // Connection object we initialized
            DB_HOST.c_str(),       // Server address as C string
            DB_USER.c_str(),       // Username as C string
            DB_PASS.c_str(),       // Password as C string
            DB_NAME.c_str(),       // Database name as C string
            0,                     // Port (0 = use default)
            nullptr,               // Unix socket (not using)
            0                      // Client flags (none)
        );
    }
    
    // Destructor - runs automatically when the object is destroyed
    ~DatabaseConnection()
    {
        // Check if we have a connection
        if (mysql_connection != nullptr)
        {
            // Close it to free resources
            mysql_close(mysql_connection);
        }
    }
};

// This function sends the HTML content-type header
// We need this before sending any HTML output
void send_html_header()
{
    // Send header with proper line endings (\r\n\r\n)
    cout << "Content-Type: text/html\r\n\r\n";
}

// This function extracts the session token from the HTTP cookie
// Cookies are sent in the HTTP_COOKIE environment variable
string get_session_token_from_cookie()
{
    // Get the cookie header from environment variables
    const char* cookie_header = getenv("HTTP_COOKIE");
    // If no cookies exist, return empty string
    if (!cookie_header) return "";
    
    // Convert to C++ string for easier manipulation
    string cookies = cookie_header;
    
    // Search for "session=" in the cookie string
    int session_pos = -1; // -1 means not found yet
    // Loop through the string looking for "session="
    for(int i = 0; i < cookies.size() - 7; i++) {
        // Extract 8 characters starting at position i
        string sub = "";
        for(int j = 0; j < 8; j++) {
            sub += cookies[i + j];
        }
        // Check if it matches "session="
        if(sub == "session=") {
            session_pos = i; // Remember where we found it
            break; // Stop searching
        }
    }
    // If "session=" not found, return empty string
    if (session_pos == -1) return "";
    
    // Extract the token value after "session="
    int start_pos = session_pos + 8; // Start after "session="
    int end_pos = -1; // Where token ends
    // Look for semicolon (separates different cookies)
    for(int i = start_pos; i < cookies.size(); i++) {
        if(cookies[i] == ';') {
            end_pos = i; // Found end of token
            break;
        }
    }
    // If no semicolon, token goes to end of string
    if (end_pos == -1) end_pos = cookies.length();
    
    // Build the token string character by character
    string result = "";
    for(int i = start_pos; i < end_pos; i++) {
        result += cookies[i];
    }
    
    // Return the extracted session token
    return result;
}

// This function validates a session and returns the user ID
// Returns true and sets out_user_id if session is valid
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get the session token from the cookie
    string session_token = get_session_token_from_cookie();
    // If no token, user is not logged in
    if (session_token.empty()) return false;
    
    // SQL query to check if session exists and is active (within last 5 minutes)
    // DATE_SUB subtracts 5 minutes from NOW() to get cutoff time
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Initialize a prepared statement for safety
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL statement
    int prep = mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql));
    // Check if preparation failed
    if (prep != 0)
    {
        // Close statement and return false
        mysql_stmt_close(session_statement);
        return false;
    }
    
    // Create a binding for the session token parameter
    MYSQL_BIND param_binding;
    param_binding.buffer_type = MYSQL_TYPE_STRING;         // It's a string
    param_binding.buffer = (void*)session_token.c_str();   // Point to token
    param_binding.buffer_length = session_token.size();    // Token length
    param_binding.is_null = nullptr;                       // Not null
    param_binding.length = nullptr;                        // No length variable
    param_binding.error = nullptr;                         // No error variable
    
    // Bind the parameter to the statement
    mysql_stmt_bind_param(session_statement, &param_binding);
    // Execute the query
    mysql_stmt_execute(session_statement);
    
    // Create a binding for the result (user_id)
    MYSQL_BIND result_binding;
    result_binding.buffer_type = MYSQL_TYPE_LONGLONG;      // Big integer
    result_binding.buffer = &out_user_id;                  // Store result here
    result_binding.is_null = nullptr;                      // Not null
    result_binding.length = nullptr;                       // No length variable
    result_binding.error = nullptr;                        // No error variable
    
    // Bind the result variable
    mysql_stmt_bind_result(session_statement, &result_binding);
    // Try to fetch a row
    int fetch_result = mysql_stmt_fetch(session_statement);
    // 0 means we found a valid session
    bool session_valid = (fetch_result == 0);
    // Close the statement
    mysql_stmt_close(session_statement);
    
    // If session is valid, update its last_active timestamp
    if (session_valid)
    {
        // SQL to update the last_active time to now
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        // Prepare an update statement
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        // Bind the session token parameter (reuse param_binding)
        mysql_stmt_bind_param(update_stmt, &param_binding);
        // Execute the update
        mysql_stmt_execute(update_stmt);
        // Close the statement
        mysql_stmt_close(update_stmt);
    }
    
    // Return whether session was valid
    return session_valid;
}

// This function gets the current user ID from their session
// Returns 0 if not logged in
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    // Variable to hold the user ID
    unsigned long long user_id = 0;
    // Try to get user from session
    bool got_user = get_user_from_session(database, user_id);
    // Return the user ID (will be 0 if not logged in)
    return user_id;
}

// This function displays the user's active bids
// Shows if they've been outbid on any auctions
void display_active_bids_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    // Start the HTML for this section
    cout << "<div class='card'><h3>Active Bids</h3>";
    
    // Complex SQL query to get user's active bids
    // It finds auctions where: 1) auction is still open, 2) user has bid, 3) shows if outbid
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
    
    // Initialize prepared statement
    MYSQL_STMT* active_bids_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(active_bids_statement, active_bids_sql, strlen(active_bids_sql));
    
    // Create bindings for the two user_id parameters (same user, used twice)
    MYSQL_BIND parameter_bindings[2];
    for(int i = 0; i < 2; i++) {
        parameter_bindings[i].buffer_type = MYSQL_TYPE_LONGLONG;  // Big integer
        parameter_bindings[i].buffer = &current_user_id;          // Both use current_user_id
        parameter_bindings[i].is_null = nullptr;                  // Not null
        parameter_bindings[i].length = nullptr;                   // No length variable
        parameter_bindings[i].error = nullptr;                    // No error variable
    }
    
    // Bind the parameters
    mysql_stmt_bind_param(active_bids_statement, parameter_bindings);
    // Execute the query
    mysql_stmt_execute(active_bids_statement);
    
    // Variables to hold the results from each row
    char auction_title_buffer[121];     // Buffer for auction title
    unsigned long title_length;         // Actual length of title
    double current_top_bid;             // Highest bid on this auction
    double my_bid_amount;               // User's bid amount
    
    // Create result bindings array (3 columns)
    MYSQL_BIND result_bindings[3];
    // Initialize all bindings to null first
    for(int i = 0; i < 3; i++) {
        result_bindings[i].buffer_type = MYSQL_TYPE_NULL;
        result_bindings[i].buffer = nullptr;
        result_bindings[i].buffer_length = 0;
        result_bindings[i].is_null = nullptr;
        result_bindings[i].length = nullptr;
        result_bindings[i].error = nullptr;
    }
    
    // Bind the title column (first column)
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Bind the current_top_bid column (second column)
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &current_top_bid;
    
    // Bind the my_bid_amount column (third column)
    result_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[2].buffer = &my_bid_amount;
    
    // Bind all results to our variables
    mysql_stmt_bind_result(active_bids_statement, result_bindings);
    
    // Start the HTML list
    cout << "<ul class='list'>";
    
    // Set output format for money (2 decimal places)
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Try to fetch first row
    int fetch = mysql_stmt_fetch(active_bids_statement);
    // Loop through all rows
    while (fetch == 0)
    {
        // Convert title buffer to C++ string
        string auction_title = "";
        for(int i = 0; i < title_length; i++) {
            auction_title += auction_title_buffer[i]; // Copy each character
        }
        
        // Check if user has been outbid
        bool user_is_outbid = false;
        if(my_bid_amount < current_top_bid) {
            user_is_outbid = true; // Someone else bid higher
        }
        
        // Start the list item
        cout << "<li>";
        // Start span for auction info
        cout << "<span>";
        // Output title in bold
        cout << "<strong>" << auction_title << "</strong>";
        
        // If user is outbid, show a warning notice
        if (user_is_outbid)
        {
            cout << " <span class='notice'>Outbid</span>";
        }
        
        // Close info span
        cout << "</span>";
        // Start span for price and action
        cout << "<span>";
        // Show current top bid with dollar sign
        cout << "$" << current_top_bid << " <a href='trade.html'>Increase bid</a>";
        // Close price span
        cout << "</span>";
        // Close list item
        cout << "</li>";
        
        // Fetch next row
        fetch = mysql_stmt_fetch(active_bids_statement);
    }
    
    // Close the HTML list
    cout << "</ul>";
    // Close the statement to free memory
    mysql_stmt_close(active_bids_statement);
    // Close the card div
    cout << "</div>";
}

// This function displays auctions where the user bid but didn't win
// Shows closed auctions where someone else had the winning bid
void display_lost_auctions_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    // Start the HTML for this section
    cout << "<div class='card'><h3>Didn't Win</h3>";
    
    // Complex SQL query to find closed auctions where user participated but lost
    // EXISTS checks if user bid on the auction
    // NOT EXISTS checks that user didn't have the winning (highest) bid
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
    
    // Initialize prepared statement
    MYSQL_STMT* lost_auctions_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(lost_auctions_statement, lost_auctions_sql, strlen(lost_auctions_sql));
    
    // Create bindings for the two user_id parameters (same user, used twice)
    MYSQL_BIND parameter_bindings[2];
    for(int i = 0; i < 2; i++) {
        parameter_bindings[i].buffer_type = MYSQL_TYPE_LONGLONG;  // Big integer
        parameter_bindings[i].buffer = &current_user_id;          // Both use current_user_id
        parameter_bindings[i].is_null = nullptr;                  // Not null
        parameter_bindings[i].length = nullptr;                   // No length variable
        parameter_bindings[i].error = nullptr;                    // No error variable
    }
    
    // Bind the parameters
    mysql_stmt_bind_param(lost_auctions_statement, parameter_bindings);
    // Execute the query
    mysql_stmt_execute(lost_auctions_statement);
    
    // Variables to hold results
    char auction_title_buffer[121];     // Buffer for auction title
    unsigned long title_length;         // Actual length of title
    double winning_amount;              // The winning bid amount
    
    // Create result bindings array (2 columns)
    MYSQL_BIND result_bindings[2];
    // Initialize all bindings
    for(int i = 0; i < 2; i++) {
        result_bindings[i].buffer_type = MYSQL_TYPE_NULL;
        result_bindings[i].buffer = nullptr;
        result_bindings[i].buffer_length = 0;
        result_bindings[i].is_null = nullptr;
        result_bindings[i].length = nullptr;
        result_bindings[i].error = nullptr;
    }
    
    // Bind the title column (first column)
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Bind the winning_amount column (second column)
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &winning_amount;
    
    // Bind the results to our variables
    mysql_stmt_bind_result(lost_auctions_statement, result_bindings);
    
    // Start the HTML list
    cout << "<ul class='list'>";
    
    // Set output format for money (2 decimal places)
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Try to fetch first row
    int fetch = mysql_stmt_fetch(lost_auctions_statement);
    // Loop through all rows
    while (fetch == 0)
    {
        // Convert title buffer to C++ string
        string auction_title = "";
        for(int i = 0; i < title_length; i++) {
            auction_title += auction_title_buffer[i]; // Copy each character
        }
        
        // Output the list item with auction info
        cout << "<li>";
        // Output title in bold within a span
        cout << "<span><strong>" << auction_title << "</strong></span>";
        // Output winning amount with dollar sign
        cout << "<span>$" << winning_amount << "</span>";
        // Close list item
        cout << "</li>";
        
        // Fetch next row
        fetch = mysql_stmt_fetch(lost_auctions_statement);
    }
    
    // Close the HTML list
    cout << "</ul>";
    // Close the statement to free memory
    mysql_stmt_close(lost_auctions_statement);
    // Close the card div
    cout << "</div>";
}

// This is the main function - where the program starts
int main()
{
    // Turn off C I/O synchronization for faster output
    ios::sync_with_stdio(false);
    
    // Send the HTML content-type header first
    send_html_header();
    
    // Create database connection (connects automatically)
    DatabaseConnection database;
    
    // Get the current logged-in user's ID
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Check if no one is logged in (user_id would be 0)
    if (current_user_id == 0)
    {
        // Send redirect status code
        cout << "Status: 303 See Other\r\n";
        // Redirect to login page
        cout << "Location: auth.html\r\n";
        // Send content type
        cout << "Content-Type: text/html\r\n\r\n";
        // Send message
        cout << "Session expired. Please log in.";
        // Exit the program
        return 0;
    }
    
    // User is logged in, show their active bids
    display_active_bids_section(database, current_user_id);
    
    // Show auctions they didn't win
    display_lost_auctions_section(database, current_user_id);
    
    // Exit the program successfully
    return 0;
}