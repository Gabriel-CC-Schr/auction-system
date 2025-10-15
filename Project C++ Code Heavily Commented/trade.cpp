// Include all standard C++ libraries (GCC convenience header)
// Provides: string, vector, map, iostream, algorithm, etc.
#include <bits/stdc++.h>

// Include MySQL C library for database operations
// Allows us to store/retrieve auction and bid data
#include <mysql/mysql.h>

// Use standard namespace to avoid "std::" prefix
using namespace std;

// Database connection constants - values never change at runtime
const string DB_HOST = "localhost";                      // Database server location
const string DB_USER = "allindragons";   				 // MySQL username
const string DB_PASS = "snogardnilla_002";               // MySQL password
const string DB_NAME = "cs370_section2_allindragons";   // Database name

/**
 * FUNCTION: send_html_header
 * PURPOSE: Send HTTP Content-Type header before HTML output
 * WHY: Required by HTTP protocol - tells browser what we're sending
 */
void send_html_header()
{
    // Send Content-Type header with double line break
    // \r\n = carriage return + line feed (HTTP line ending)
    // Two of them separate headers from body
    cout << "Content-Type: text/html\r\n\r\n";
}

/**
 * FUNCTION: url_decode
 * PURPOSE: Convert URL-encoded text to normal text
 * EXAMPLE: "Hello+World%21" becomes "Hello World!"
 * WHY: Browsers encode special characters in form data
 */
string url_decode(const string &encoded_string)
{
    // Create empty string for decoded output
    string output_string;
    
    // Pre-allocate memory (optimization)
    output_string.reserve(encoded_string.size());
    
    // Loop through each character of encoded string
    for (size_t current_index = 0; current_index < encoded_string.size(); ++current_index)
    {
        // Plus sign represents space in URL encoding
        if (encoded_string[current_index] == '+')
        {
            // Add space to output
            output_string += ' ';
        }
        // Percent sign starts hex encoding (%20 = space)
        else if (encoded_string[current_index] == '%' && 
                 current_index + 2 < encoded_string.size())
        {
            // Get two hex digits after %
            string hex_string = encoded_string.substr(current_index + 1, 2);
            
            // Convert hex to integer to character
            // strtol converts string to long integer
            // Base 16 means hexadecimal
            char decoded_char = static_cast<char>(strtol(hex_string.c_str(), nullptr, 16));
            
            // Add decoded character to output
            output_string += decoded_char;
            
            // Skip next 2 characters (already processed)
            current_index += 2;
        }
        else
        {
            // Regular character - copy as-is
            output_string += encoded_string[current_index];
        }
    }
    
    // Return completed decoded string
    return output_string;
}

/**
 * FUNCTION: parse_post_data
 * PURPOSE: Parse form data from POST request into key-value pairs
 * EXAMPLE: "name=John&age=25" becomes map{"name":"John", "age":"25"}
 * WHY: Form data comes as one encoded string, we need individual fields
 */
map<string, string> parse_post_data()
{
    // Get CONTENT_LENGTH environment variable
    // Tells us how many bytes of POST data to read
    const char* content_length_env = getenv("CONTENT_LENGTH");
    
    // If CONTENT_LENGTH exists, use it; else use "0"
    string content_length_string = content_length_env ? content_length_env : "0";
    
    // Convert string to integer
    int content_length = atoi(content_length_string.c_str());
    
    // Create string with space for all POST data
    // max(0, content_length) prevents negative sizes
    string post_body(max(0, content_length), '\0');
    
    // If there's data to read
    if (content_length > 0)
    {
        // Read exact number of bytes from stdin
        // Web server pipes POST data through stdin
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    // Create map to store key-value pairs
    map<string, string> form_data;
    
    // Start at beginning of POST data
    size_t current_position = 0;
    
    // Parse until we reach end of data
    while (current_position < post_body.size())
    {
        // Find equals sign (separates key from value)
        size_t equals_position = post_body.find('=', current_position);
        
        // If no equals sign, we're done
        if (equals_position == string::npos) break;
        
        // Find ampersand (separates different pairs)
        size_t ampersand_position = post_body.find('&', equals_position);
        
        // Extract key portion
        string raw_key = post_body.substr(current_position, equals_position - current_position);
        
        // Decode the key
        string decoded_key = url_decode(raw_key);
        
        // Calculate value start position
        size_t value_start = equals_position + 1;
        
        // Calculate value end position
        size_t value_end = (ampersand_position == string::npos) ? post_body.size() : ampersand_position;
        
        // Extract value portion
        string raw_value = post_body.substr(value_start, value_end - value_start);
        
        // Decode the value
        string decoded_value = url_decode(raw_value);
        
        // Store key-value pair in map
        form_data[decoded_key] = decoded_value;
        
        // If no more pairs, exit loop
        if (ampersand_position == string::npos) break;
        
        // Move to start of next pair
        current_position = ampersand_position + 1;
    }
    
    // Return completed map
    return form_data;
}

/**
 * FUNCTION: get_session_token_from_cookie
 * PURPOSE: Extract session token from HTTP cookies
 * RETURNS: Session token string, or empty if not found
 * WHY: Need token to validate user is logged in
 */
string get_session_token_from_cookie()
{
    // Get HTTP_COOKIE environment variable
    // Contains all cookies browser sent
    const char* cookie_header = getenv("HTTP_COOKIE");
    
    // If no cookies, return empty string
    if (!cookie_header) return "";
    
    // Convert to C++ string
    string cookies(cookie_header);
    
    // Search for "session=" in cookies
    size_t session_pos = cookies.find("session=");
    
    // If not found, return empty
    if (session_pos == string::npos) return "";
    
    // Calculate start of token value
    // "session=" is 8 characters
    size_t start_pos = session_pos + 8;
    
    // Find semicolon marking end of cookie
    size_t end_pos = cookies.find(';', start_pos);
    
    // If no semicolon, token goes to end
    if (end_pos == string::npos) end_pos = cookies.length();
    
    // Extract and return token
    return cookies.substr(start_pos, end_pos - start_pos);
}

/**
 * FUNCTION: get_user_from_session
 * PURPOSE: Validate session and get user ID
 * RETURNS: true if valid, false if expired/invalid
 * WHY: Protect pages - only logged-in users can trade
 */
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get session token from cookies
    string session_token = get_session_token_from_cookie();
    
    // If no token, not logged in
    if (session_token.empty()) return false;
    
    // SQL to find valid session
    // Session must exist and be active within last 5 minutes
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Create prepared statement
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL query
    if (mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql)))
    {
        // Preparation failed - close and return false
        mysql_stmt_close(session_statement);
        return false;
    }
    
    // Create parameter binding for token
    MYSQL_BIND param_binding{};
    param_binding.buffer_type = MYSQL_TYPE_STRING;          // It's a string
    param_binding.buffer = (void*)session_token.c_str();    // Point to token
    param_binding.buffer_length = session_token.size();     // Token length
    
    // Bind parameter to statement
    mysql_stmt_bind_param(session_statement, &param_binding);
    
    // Execute query
    mysql_stmt_execute(session_statement);
    
    // Create result binding for user_id
    MYSQL_BIND result_binding{};
    result_binding.buffer_type = MYSQL_TYPE_LONGLONG;       // It's a big integer
    result_binding.buffer = &out_user_id;                   // Store result here
    
    // Bind result structure
    mysql_stmt_bind_result(session_statement, &result_binding);
    
    // Fetch result - returns 0 if row found
    bool session_valid = (mysql_stmt_fetch(session_statement) == 0);
    
    // Close statement
    mysql_stmt_close(session_statement);
    
    // If session valid, update activity timestamp
    if (session_valid)
    {
        // SQL to update last_active
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        
        // Create update statement
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        
        // Prepare update statement
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        
        // Bind token parameter
        mysql_stmt_bind_param(update_stmt, &param_binding);
        
        // Execute update
        mysql_stmt_execute(update_stmt);
        
        // Close update statement
        mysql_stmt_close(update_stmt);
    }
    
    // Return validation result
    return session_valid;
}

/**
 * STRUCT: DatabaseConnection
 * PURPOSE: Manage database connection lifecycle automatically
 * WHY: RAII pattern - connects on creation, disconnects on destruction
 */
struct DatabaseConnection
{
    // Pointer to MySQL connection
    MYSQL* mysql_connection = nullptr;
    
    // CONSTRUCTOR - runs when object is created
    DatabaseConnection()
    {
        // Initialize MySQL connection structure
        mysql_connection = mysql_init(nullptr);
        
        // Connect to MySQL server
        mysql_real_connect(
            mysql_connection,       // Connection object
            DB_HOST.c_str(),       // Server hostname
            DB_USER.c_str(),       // Username
            DB_PASS.c_str(),       // Password
            DB_NAME.c_str(),       // Database name
            0,                     // Port (0=default 3306)
            nullptr,               // Unix socket
            0                      // Flags
        );
    }
    
    // DESTRUCTOR - runs when object is destroyed
    ~DatabaseConnection()
    {
        // If connection exists, close it
        if (mysql_connection != nullptr)
        {
            mysql_close(mysql_connection);
        }
    }
};

/**
 * FUNCTION: handle_auction_options_request
 * PURPOSE: Generate HTML <option> elements for auction dropdown
 * WHY: AJAX request from trade.html needs list of open auctions
 */
void handle_auction_options_request(DatabaseConnection& database)
{
    // SQL to get all open auctions
    // Only show auctions that haven't ended yet
    const char* auction_options_sql = 
        "SELECT id, title FROM auctions "
        "WHERE status = 'OPEN' AND end_time > NOW() "
        "ORDER BY end_time ASC";
    
    // Create prepared statement
    MYSQL_STMT* options_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(options_statement, auction_options_sql, strlen(auction_options_sql));
    
    // Execute query
    mysql_stmt_execute(options_statement);
    
    // Variables to receive results
    unsigned long long auction_id;              // Auction ID
    char auction_title_buffer[121];             // Title buffer
    unsigned long title_length;                 // Actual title length
    
    // Create result bindings
    MYSQL_BIND result_bindings[2]{};
    
    // Bind auction ID
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &auction_id;
    
    // Bind title
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = auction_title_buffer;
    result_bindings[1].buffer_length = 120;
    result_bindings[1].length = &title_length;
    
    // Bind results to statement
    mysql_stmt_bind_result(options_statement, result_bindings);
    
    // Send HTTP header
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Loop through each auction
    while (mysql_stmt_fetch(options_statement) == 0)
    {
        // Convert title to C++ string
        string auction_title(auction_title_buffer, title_length);
        
        // Output HTML option element
        // value = auction ID (what gets submitted)
        // text = auction title (what user sees)
        cout << "<option value='" << auction_id << "'>" << auction_title << "</option>";
    }
    
    // Close statement
    mysql_stmt_close(options_statement);
}

/**
 * FUNCTION: handle_sell_action
 * PURPOSE: Create new auction listing
 * WHY: Users need to be able to sell items
 */
void handle_sell_action(DatabaseConnection& database, 
                       unsigned long long seller_user_id,
                       const map<string, string>& form_data)
{
    // Extract form fields from submitted data
    string auction_title = form_data.at("title");
    string auction_description = form_data.at("description");
    string start_price_string = form_data.at("start_price");
    string start_time = form_data.at("start_time");
    
    // Truncate description if too long
    // Database can handle more, but display might be limited
    if (auction_description.length() > 4000)
    {
        // Keep first 4000 chars and add ellipsis
        auction_description = auction_description.substr(0, 4000) + "...";
    }
    
    // Validate all required fields are present
    if (auction_title.empty() || auction_description.empty() || 
        start_price_string.empty() || start_time.empty())
    {
        // Send error message
        send_html_header();
        cout << "<p>Missing fields.</p>";
        return;
    }
    
    // Convert price string to decimal number
    // stod = "string to double"
    double starting_price = stod(start_price_string);
    
    // SQL to insert new auction
    // end_time is automatically calculated as start_time + 168 hours (7 days)
    const char* insert_auction_sql = 
        "INSERT INTO auctions(seller_id, title, description, start_price, start_time, end_time, status) "
        "VALUES(?, ?, ?, ?, ?, DATE_ADD(?, INTERVAL 168 HOUR), 'OPEN')";
    
    // Create prepared statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(insert_statement, insert_auction_sql, strlen(insert_auction_sql));
    
    // Create parameter bindings array (6 parameters)
    MYSQL_BIND parameter_bindings[6]{};
    
    // Parameter 0: seller_id (who's creating the auction)
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &seller_user_id;
    
    // Parameter 1: title
    parameter_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[1].buffer = (void*)auction_title.c_str();
    parameter_bindings[1].buffer_length = auction_title.size();
    
    // Parameter 2: description
    parameter_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[2].buffer = (void*)auction_description.c_str();
    parameter_bindings[2].buffer_length = auction_description.size();
    
    // Parameter 3: start_price
    parameter_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[3].buffer = &starting_price;
    
    // Parameter 4: start_time (for start_time column)
    parameter_bindings[4].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[4].buffer = (void*)start_time.c_str();
    parameter_bindings[4].buffer_length = start_time.size();
    
    // Parameter 5: start_time again (for end_time calculation)
    parameter_bindings[5].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[5].buffer = (void*)start_time.c_str();
    parameter_bindings[5].buffer_length = start_time.size();
    
    // Bind all parameters
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    
    // Send HTML header
    send_html_header();
    
    // Try to execute insert
    if (mysql_stmt_execute(insert_statement) != 0)
    {
        // Insert failed - show error
        cout << "<p>Error listing item.</p>";
    }
    else
    {
        // Insert succeeded - show success message
        cout << "<p>Item listed for 7 days. <a href='listings.html'>View listings</a></p>";
    }
    
    // Close statement
    mysql_stmt_close(insert_statement);
}

/**
 * FUNCTION: validate_auction_for_bidding
 * PURPOSE: Check if user can bid on specified auction
 * RETURNS: true if valid, false if not
 * WHY: Prevent self-bidding and bidding on closed auctions
 */
bool validate_auction_for_bidding(DatabaseConnection& database,
                                  unsigned long long auction_id,
                                  unsigned long long bidder_user_id)
{
    // SQL to get auction details
    // We need seller_id to prevent self-bidding
    // We need status and end_time to ensure auction is still open
    const char* auction_check_sql = 
        "SELECT seller_id, status, end_time FROM auctions WHERE id = ?";
    
    // Create prepared statement
    MYSQL_STMT* check_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(check_statement, auction_check_sql, strlen(auction_check_sql));
    
    // Create parameter binding for auction_id
    MYSQL_BIND parameter_binding{};
    parameter_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_binding.buffer = &auction_id;
    
    // Bind parameter
    mysql_stmt_bind_param(check_statement, &parameter_binding);
    
    // Execute query
    mysql_stmt_execute(check_statement);
    
    // Variables to receive results
    unsigned long long seller_id;           // Who owns the auction
    char status_buffer[6];                  // Auction status
    char end_time_buffer[20];               // When auction ends
    unsigned long status_length;            // Actual status length
    unsigned long end_time_length;          // Actual end_time length
    
    // Create result bindings
    MYSQL_BIND result_bindings[3]{};
    
    // Bind seller_id
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &seller_id;
    
    // Bind status
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = status_buffer;
    result_bindings[1].buffer_length = 6;
    result_bindings[1].length = &status_length;
    
    // Bind end_time
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[2].buffer = end_time_buffer;
    result_bindings[2].buffer_length = 20;
    result_bindings[2].length = &end_time_length;
    
    // Bind results
    mysql_stmt_bind_result(check_statement, result_bindings);
    
    // Try to fetch auction details
    if (mysql_stmt_fetch(check_statement) != 0)
    {
        // Auction not found
        send_html_header();
        cout << "<p>Auction not found.</p>";
        mysql_stmt_close(check_statement);
        return false;
    }
    
    // Close statement
    mysql_stmt_close(check_statement);
    
    // Check if user is trying to bid on own auction
    if (seller_id == bidder_user_id)
    {
        // Self-bidding not allowed
        send_html_header();
        cout << "<p>You cannot bid on your own item.</p>";
        return false;
    }
    
    // All validations passed
    return true;
}

/**
 * FUNCTION: get_current_top_bid
 * PURPOSE: Find highest bid for an auction
 * RETURNS: Current top bid amount (or start_price if no bids)
 * WHY: Need to ensure new bids are higher than current top
 */
double get_current_top_bid(DatabaseConnection& database, unsigned long long auction_id)
{
    // SQL to get maximum bid or starting price
    // GREATEST returns larger of two values
    // IFNULL returns second value if first is NULL
    // If no bids exist, MAX returns NULL, IFNULL returns 0
    // GREATEST then compares 0 vs start_price
    const char* top_bid_sql = 
        "SELECT GREATEST(IFNULL(MAX(amount), 0), (SELECT start_price FROM auctions WHERE id = ?)) "
        "FROM bids WHERE auction_id = ?";
    
    // Create prepared statement
    MYSQL_STMT* top_bid_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(top_bid_statement, top_bid_sql, strlen(top_bid_sql));
    
    // Create parameter bindings (auction_id used twice)
    MYSQL_BIND parameter_bindings[2]{};
    
    // First occurrence of auction_id
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &auction_id;
    
    // Second occurrence - same as first
    parameter_bindings[1] = parameter_bindings[0];
    
    // Bind parameters
    mysql_stmt_bind_param(top_bid_statement, parameter_bindings);
    
    // Execute query
    mysql_stmt_execute(top_bid_statement);
    
    // Variable to receive top bid amount
    double top_bid_amount;
    
    // Create result binding
    MYSQL_BIND result_binding{};
    result_binding.buffer_type = MYSQL_TYPE_DOUBLE;
    result_binding.buffer = &top_bid_amount;
    
    // Bind result
    mysql_stmt_bind_result(top_bid_statement, &result_binding);
    
    // Fetch result (should always get one row)
    mysql_stmt_fetch(top_bid_statement);
    
    // Close statement
    mysql_stmt_close(top_bid_statement);
    
    // Return the top bid amount
    return top_bid_amount;
}

/**
 * FUNCTION: place_bid_in_database
 * PURPOSE: Insert new bid into database
 * RETURNS: true if successful, false if error
 * WHY: Record bid for history and tracking
 */
bool place_bid_in_database(DatabaseConnection& database,
                           unsigned long long auction_id,
                           unsigned long long bidder_user_id,
                           double bid_amount)
{
    // SQL to insert new bid
    const char* insert_bid_sql = 
        "INSERT INTO bids(auction_id, bidder_id, amount) VALUES(?, ?, ?)";
    
    // Create prepared statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare SQL
    mysql_stmt_prepare(insert_statement, insert_bid_sql, strlen(insert_bid_sql));
    
    // Create parameter bindings (3 parameters)
    MYSQL_BIND parameter_bindings[3]{};
    
    // Parameter 0: auction_id
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &auction_id;
    
    // Parameter 1: bidder_id
    parameter_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[1].buffer = &bidder_user_id;
    
    // Parameter 2: amount
    parameter_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[2].buffer = &bid_amount;
    
    // Bind parameters
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    
    // Execute insert
    // Returns 0 on success
    bool insert_successful = (mysql_stmt_execute(insert_statement) == 0);
    
    // Close statement
    mysql_stmt_close(insert_statement);
    
    // Return success status
    return insert_successful;
}

/**
 * FUNCTION: handle_bid_action
 * PURPOSE: Process user's bid on an auction
 * WHY: Core functionality - users bid to buy items
 */
void handle_bid_action(DatabaseConnection& database,
                       unsigned long long bidder_user_id,
                       const map<string, string>& form_data)
{
    // Extract auction ID from form
    // strtoull = "string to unsigned long long"
    // nullptr = no error checking
    // 10 = base 10 (decimal)
    unsigned long long auction_id = strtoull(form_data.at("auction_id").c_str(), nullptr, 10);
    
    // Extract bid amount and convert to decimal
    double bid_amount = stod(form_data.at("amount"));
    
    // Validate auction exists and user can bid
    if (!validate_auction_for_bidding(database, auction_id, bidder_user_id))
    {
        // Validation failed - error already shown
        return;
    }
    
    // Get current top bid for this auction
    double current_top_bid = get_current_top_bid(database, auction_id);
    
    // Check if new bid is higher than current top
    if (bid_amount <= current_top_bid)
    {
        // Bid too low - show error
        send_html_header();
        cout << "<p>Bid must be greater than current top ($" 
             << fixed << setprecision(2) << current_top_bid << ").</p>";
        return;
    }
    
    // Send HTML header
    send_html_header();
    
    // Try to place the bid
    if (!place_bid_in_database(database, auction_id, bidder_user_id, bid_amount))
    {
        // Database error
        cout << "<p>Error placing bid.</p>";
    }
    else
    {
        // Success!
        cout << "<p>Bid placed! <a href='transactions.html'>View my bids</a></p>";
    }
}

/**
 * FUNCTION: main
 * PURPOSE: Main entry point for trading CGI program
 * HANDLES: Auction options AJAX, selling items, placing bids
 * WHY: Users need to interact with auctions
 */
int main()
{
    // Turn off C/C++ I/O sync for speed
    ios::sync_with_stdio(false);
    
    // Create database connection
    DatabaseConnection database;
    
    // Get query string (URL parameters)
    // Example: trade.cgi?action=options
    string query_string = getenv("QUERY_STRING") ? getenv("QUERY_STRING") : "";
    
    // Check if this is AJAX request for auction options
    if (query_string.find("action=options") != string::npos)
    {
        // Handle options request and exit
        handle_auction_options_request(database);
        return 0;
    }
    
    // For sell/bid actions, need valid session
    unsigned long long current_user_id;
    
    // Validate user session
    if (!get_user_from_session(database, current_user_id))
    {
        // Session invalid or expired
        send_html_header();
        cout << "<div class='notice'>Session expired (5 minutes idle). "
             << "<a href='auth.html'>Sign in</a>.</div>";
        return 0;
    }
    
    // Parse POST form data
    auto form_data = parse_post_data();
    
    // Get requested action
    string requested_action = form_data["action"];
    
    // Handle sell action
    if (requested_action == "sell")
    {
        // Create new auction
        handle_sell_action(database, current_user_id, form_data);
        return 0;
    }
    
    // Handle bid action
    if (requested_action == "bid")
    {
        // Place bid on auction
        handle_bid_action(database, current_user_id, form_data);
        return 0;
    }
    
    // Unknown action
    send_html_header();
    cout << "<p>Unsupported action.</p>";
    
    // Exit successfully
    return 0;
}