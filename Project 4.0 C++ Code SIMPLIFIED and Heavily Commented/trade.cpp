// *******
// trade.cpp
// *******


// This header gives us access to common C++ features like cout, string, map, etc.
#include <bits/stdc++.h>
// This header provides MySQL database functionality
#include <mysql/mysql.h>

// This lets us use standard library features without typing "std::" every time
using namespace std;

// Database connection constants - these tell us how to connect to our MySQL database
const string DB_HOST = "localhost";        // Server location (localhost = this computer)
const string DB_USER = "allindragons";     // Database username
const string DB_PASS = "snogardnilla_002"; // Database password
const string DB_NAME = "cs370_section2_allindragons"; // Which database to use

// This struct manages our database connection
// It connects when created and disconnects when destroyed (RAII pattern)
struct DatabaseConnection
{
    // Pointer to hold the MySQL connection object
    MYSQL* mysql_connection;
    
    // Constructor - runs when we create a DatabaseConnection
    DatabaseConnection()
    {
        // Set pointer to null initially
        mysql_connection = nullptr;
        // Initialize MySQL library
        mysql_connection = mysql_init(nullptr);
        
        // Actually connect to the database
        MYSQL* result = mysql_real_connect(
            mysql_connection,      // Connection object
            DB_HOST.c_str(),       // Server address as C string
            DB_USER.c_str(),       // Username as C string
            DB_PASS.c_str(),       // Password as C string
            DB_NAME.c_str(),       // Database name as C string
            0,                     // Port (0 = default)
            nullptr,               // Unix socket (not using)
            0                      // Client flags (none)
        );
    }
    
    // Destructor - runs automatically when object is destroyed
    ~DatabaseConnection()
    {
        // Check if connection exists
        if (mysql_connection != nullptr)
        {
            // Close it to free resources
            mysql_close(mysql_connection);
        }
    }
};

// This function sends the HTML content-type header
// We need this before sending any HTML content
void send_html_header()
{
    // Send the header with proper line endings
    cout << "Content-Type: text/html\r\n\r\n";
}

// This function decodes URL-encoded strings
// Example: "hello+world" becomes "hello world", "%20" becomes space
string url_decode(const string &encoded_string)
{
    // Create empty string to build our result
    string output_string = "";
    
    // Start at the first character
    int current_index = 0;
    // Loop through every character in the input
    while(current_index < encoded_string.size())
    {
        // Get the current character
        char c = encoded_string[current_index];
        // Check if it's a plus sign (means space in URL encoding)
        if (c == '+')
        {
            // Add a space to output
            output_string += ' ';
            // Move to next character
            current_index++;
        }
        // Check if it's a percent sign (starts hex code like %20)
        else if (c == '%')
        {
            // Make sure there are 2 more characters after the %
            if(current_index + 2 < encoded_string.size()) {
                // Build a string with the 2 hex digits
                string hex_string = "";
                hex_string += encoded_string[current_index + 1]; // First hex digit
                hex_string += encoded_string[current_index + 2]; // Second hex digit
                // Convert hex string to a character
                char decoded_char = (char)strtol(hex_string.c_str(), nullptr, 16);
                // Add the decoded character to output
                output_string += decoded_char;
                // Skip past the % and 2 hex digits
                current_index += 3;
            } else {
                // Not enough characters, just skip the %
                current_index++;
            }
        }
        // Regular character (no special encoding)
        else
        {
            // Add it directly to output
            output_string += c;
            // Move to next character
            current_index++;
        }
    }
    
    // Return the fully decoded string
    return output_string;
}

// This function reads and parses POST form data
// Returns a map (like a dictionary) of field name -> value
map<string, string> parse_post_data()
{
    // Get the content length from environment variable
    const char* content_length_env = getenv("CONTENT_LENGTH");
    // Store it in a string (default to "0" if not set)
    string content_length_string = "0";
    if(content_length_env) {
        content_length_string = content_length_env;
    }
    // Convert the string to an integer
    int content_length = atoi(content_length_string.c_str());
    
    // Create a string to hold the POST data
    string post_body = "";
    // Only read if there's actually data
    if(content_length > 0) {
        // Make the string big enough
        for(int i = 0; i < content_length; i++) {
            post_body += '\0'; // Add null characters as placeholders
        }
        // Actually read the data from standard input
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    // Create a map to store the parsed fields
    map<string, string> form_data;
    // Start at position 0
    int current_position = 0;
    
    // Keep parsing until we've processed everything
    while (current_position < post_body.size())
    {
        // Find the next equals sign (separates key from value)
        int equals_position = -1; // -1 means not found
        // Search for the equals sign
        for(int i = current_position; i < post_body.size(); i++) {
            if(post_body[i] == '=') {
                equals_position = i;
                break; // Found it, stop searching
            }
        }
        // If no equals sign, we're done
        if (equals_position == -1) break;
        
        // Find the next ampersand (separates different fields)
        int ampersand_position = -1; // -1 means not found
        // Search for the ampersand
        for(int i = equals_position; i < post_body.size(); i++) {
            if(post_body[i] == '&') {
                ampersand_position = i;
                break; // Found it, stop searching
            }
        }
        
        // Extract the key (field name) before the equals sign
        string raw_key = "";
        for(int i = current_position; i < equals_position; i++) {
            raw_key += post_body[i]; // Copy each character
        }
        // Decode the key in case it's URL encoded
        string decoded_key = url_decode(raw_key);
        
        // Extract the value after the equals sign
        int value_start = equals_position + 1; // Start right after =
        int value_end; // Where value ends
        if(ampersand_position == -1) {
            value_end = post_body.size(); // Goes to end if no ampersand
        } else {
            value_end = ampersand_position; // Ends at ampersand
        }
        string raw_value = "";
        for(int i = value_start; i < value_end; i++) {
            raw_value += post_body[i]; // Copy each character
        }
        // Decode the value in case it's URL encoded
        string decoded_value = url_decode(raw_value);
        
        // Store the key-value pair in our map
        form_data[decoded_key] = decoded_value;
        
        // If no ampersand, this was the last field
        if (ampersand_position == -1) break;
        // Otherwise move past the ampersand to the next field
        current_position = ampersand_position + 1;
    }
    
    // Return the map with all the form data
    return form_data;
}

// This function extracts the session token from the HTTP cookie
// Cookies are sent in the HTTP_COOKIE environment variable
string get_session_token_from_cookie()
{
    // Get the cookie header from environment
    const char* cookie_header = getenv("HTTP_COOKIE");
    // If no cookies, return empty string
    if (!cookie_header) return "";
    
    // Convert to C++ string
    string cookies = cookie_header;
    
    // Look for "session=" in the cookie string
    int session_pos = -1; // -1 means not found
    // Search through the cookie string
    for(int i = 0; i < cookies.size() - 7; i++) {
        // Check if we found "session="
        if(cookies.substr(i, 8) == "session=") {
            session_pos = i;
            break; // Found it, stop searching
        }
    }
    // If "session=" not found, return empty string
    if (session_pos == -1) return "";
    
    // Extract the token value after "session="
    int start_pos = session_pos + 8; // Start after "session="
    int end_pos = -1; // -1 means not found
    // Look for semicolon (separates different cookies)
    for(int i = start_pos; i < cookies.size(); i++) {
        if(cookies[i] == ';') {
            end_pos = i;
            break; // Found it, stop searching
        }
    }
    // If no semicolon, token goes to end of string
    if (end_pos == -1) end_pos = cookies.length();
    
    // Build the token string
    string token = "";
    for(int i = start_pos; i < end_pos; i++) {
        token += cookies[i]; // Copy each character
    }
    
    // Return the extracted token
    return token;
}

// This function checks if a user has a valid session
// Returns true and sets out_user_id if session is valid
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get the session token from the cookie
    string session_token = get_session_token_from_cookie();
    // If no token, user is not logged in
    if (session_token.empty()) return false;
    
    // SQL query to check if session exists and is recent (within 5 minutes)
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Initialize a prepared statement
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL statement
    int prep = mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql));
    // If preparation failed, return false
    if (prep != 0)
    {
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
    result_binding.buffer = &out_user_id;                  // Store here
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
    
    // If session is valid, update its last_active time
    if (session_valid)
    {
        // SQL to update the last_active timestamp
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

// This function generates HTML <option> elements for all open auctions
// Used by the auction dropdown on the bidding form
void handle_auction_options_request(DatabaseConnection& database)
{
    // SQL query to get all open auctions
    const char* auction_options_sql = 
        "SELECT id, title FROM auctions "
        "WHERE status = 'OPEN' AND end_time > NOW() "
        "ORDER BY end_time ASC";
    
    // Initialize prepared statement
    MYSQL_STMT* options_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the query
    mysql_stmt_prepare(options_statement, auction_options_sql, strlen(auction_options_sql));
    // Execute it
    mysql_stmt_execute(options_statement);
    
    // Variables to hold results
    unsigned long long auction_id;          // Auction ID number
    char auction_title_buffer[121];         // Buffer for title
    unsigned long title_length;             // Actual title length
    
    // Create result bindings array
    MYSQL_BIND result_bindings[2]; // We're getting 2 columns
    // Initialize the bindings
    for(int i = 0; i < 2; i++) {
        result_bindings[i].buffer_type = MYSQL_TYPE_NULL;   // Set to null initially
        result_bindings[i].buffer = nullptr;                // No buffer yet
        result_bindings[i].buffer_length = 0;               // No length yet
        result_bindings[i].is_null = nullptr;               // Not null indicator
        result_bindings[i].length = nullptr;                // No length variable
        result_bindings[i].error = nullptr;                 // No error variable
    }
    
    // Bind auction ID column
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;   // Big integer
    result_bindings[0].buffer = &auction_id;                // Store here
    
    // Bind title column
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;     // String type
    result_bindings[1].buffer = auction_title_buffer;       // Store in buffer
    result_bindings[1].buffer_length = 120;                 // Max buffer size
    result_bindings[1].length = &title_length;              // Actual length goes here
    
    // Bind the results to our variables
    mysql_stmt_bind_result(options_statement, result_bindings);
    
    // Send HTTP header
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Try to fetch first row
    int fetch = mysql_stmt_fetch(options_statement);
    // Loop through all rows
    while (fetch == 0)
    {
        // Convert buffer to C++ string
        string auction_title = "";
        for(int i = 0; i < title_length; i++) {
            auction_title += auction_title_buffer[i];
        }
        // Output an HTML option element
        cout << "<option value='" << auction_id << "'>" << auction_title << "</option>";
        // Get next row
        fetch = mysql_stmt_fetch(options_statement);
    }
    
    // Close the statement
    mysql_stmt_close(options_statement);
}

// This function handles creating a new auction (selling an item)
void handle_sell_action(DatabaseConnection& database, 
                       unsigned long long seller_user_id,
                       const map<string, string>& form_data)
{
    // Initialize strings to hold form field values
    string auction_title = "";
    string auction_description = "";
    string start_price_string = "";
    string start_time = "";
    
    // Extract values from the form data map (check if keys exist first)
    if(form_data.find("title") != form_data.end()) {
        auction_title = form_data.at("title");
    }
    if(form_data.find("description") != form_data.end()) {
        auction_description = form_data.at("description");
    }
    if(form_data.find("start_price") != form_data.end()) {
        start_price_string = form_data.at("start_price");
    }
    if(form_data.find("start_time") != form_data.end()) {
        start_time = form_data.at("start_time");
    }
    
    // Validate that all required fields are present
    bool title_ok = !auction_title.empty();           // Title can't be empty
    bool desc_ok = !auction_description.empty();      // Description can't be empty
    bool price_ok = !start_price_string.empty();      // Price can't be empty
    bool time_ok = !start_time.empty();               // Time can't be empty
    
    // If any field is missing, show error
    if (!title_ok || !desc_ok || !price_ok || !time_ok)
    {
        send_html_header();
        cout << "<p>Missing fields.</p>";
        return; // Exit the function
    }
    
    // Convert price string to a double (decimal number)
    double starting_price = stod(start_price_string);
    
    // SQL to insert new auction (7 days = 168 hours)
    // DATE_ADD adds 168 hours to the start time to get end time
    const char* insert_auction_sql = 
        "INSERT INTO auctions(seller_id, title, description, start_price, start_time, end_time, status) "
        "VALUES(?, ?, ?, ?, ?, DATE_ADD(?, INTERVAL 168 HOUR), 'OPEN')";
    
    // Initialize a prepared statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(insert_statement, insert_auction_sql, strlen(insert_auction_sql));
    
    // Create bindings array (6 parameters in our SQL)
    MYSQL_BIND parameter_bindings[6];
    // Initialize all bindings to null first
    for(int i = 0; i < 6; i++) {
        parameter_bindings[i].buffer_type = MYSQL_TYPE_NULL;
        parameter_bindings[i].buffer = nullptr;
        parameter_bindings[i].buffer_length = 0;
        parameter_bindings[i].is_null = nullptr;
        parameter_bindings[i].length = nullptr;
        parameter_bindings[i].error = nullptr;
    }
    
    // Make a copy of seller_user_id (we need a variable to point to)
    unsigned long long seller_copy = seller_user_id;
    // Bind seller_id parameter (first ?)
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &seller_copy;
    
    // Bind title parameter (second ?)
    parameter_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[1].buffer = (void*)auction_title.c_str();
    parameter_bindings[1].buffer_length = auction_title.size();
    
    // Bind description parameter (third ?)
    parameter_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[2].buffer = (void*)auction_description.c_str();
    parameter_bindings[2].buffer_length = auction_description.size();
    
    // Bind starting_price parameter (fourth ?)
    parameter_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[3].buffer = &starting_price;
    
    // Bind start_time parameter (fifth ?)
    parameter_bindings[4].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[4].buffer = (void*)start_time.c_str();
    parameter_bindings[4].buffer_length = start_time.size();
    
    // Bind start_time again (sixth ? - used for calculating end_time)
    parameter_bindings[5].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[5].buffer = (void*)start_time.c_str();
    parameter_bindings[5].buffer_length = start_time.size();
    
    // Bind all the parameters to the statement
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    
    // Send HTML header
    send_html_header();
    
    // Execute the insert statement
    int exec_result = mysql_stmt_execute(insert_statement);
    // Check if it failed (non-zero means error)
    if (exec_result != 0)
    {
        cout << "<p>Error listing item.</p>";
    }
    else
    {
        // Success! Show confirmation message
        cout << "<p>Item listed for 7 days. <a href='listings.html'>View listings</a></p>";
    }
    
    // Close the statement to free memory
    mysql_stmt_close(insert_statement);
}

// This function validates that a user can bid on an auction
// Returns false if user can't bid (with error message already sent)
bool validate_auction_for_bidding(DatabaseConnection& database,
                                  unsigned long long auction_id,
                                  unsigned long long bidder_user_id)
{
    // SQL to check if auction exists and get its details
    const char* auction_check_sql = 
        "SELECT seller_id, status, end_time FROM auctions WHERE id = ?";
    
    // Initialize prepared statement
    MYSQL_STMT* check_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(check_statement, auction_check_sql, strlen(auction_check_sql));
    
    // Create binding for auction_id parameter
    MYSQL_BIND parameter_binding;
    parameter_binding.buffer_type = MYSQL_TYPE_LONGLONG;    // Big integer
    parameter_binding.buffer = &auction_id;                 // Point to auction_id
    parameter_binding.is_null = nullptr;                    // Not null
    parameter_binding.length = nullptr;                     // No length variable
    parameter_binding.error = nullptr;                      // No error variable
    // Bind the parameter
    mysql_stmt_bind_param(check_statement, &parameter_binding);
    
    // Execute the query
    mysql_stmt_execute(check_statement);
    
    // Variables to hold the results
    unsigned long long seller_id;           // Who is selling this item
    char status_buffer[6];                  // Auction status (OPEN or CLOSED)
    char end_time_buffer[20];               // When auction ends
    unsigned long status_length;            // Actual length of status
    unsigned long end_time_length;          // Actual length of end_time
    
    // Create result bindings array (3 columns)
    MYSQL_BIND result_bindings[3];
    // Initialize all bindings
    for(int i = 0; i < 3; i++) {
        result_bindings[i].buffer_type = MYSQL_TYPE_NULL;
        result_bindings[i].buffer = nullptr;
        result_bindings[i].buffer_length = 0;
        result_bindings[i].is_null = nullptr;
        result_bindings[i].length = nullptr;
        result_bindings[i].error = nullptr;
    }
    
    // Bind seller_id result (first column)
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &seller_id;
    
    // Bind status result (second column)
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = status_buffer;
    result_bindings[1].buffer_length = 6;
    result_bindings[1].length = &status_length;
    
    // Bind end_time result (third column)
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[2].buffer = end_time_buffer;
    result_bindings[2].buffer_length = 20;
    result_bindings[2].length = &end_time_length;
    
    // Bind the results to our variables
    mysql_stmt_bind_result(check_statement, result_bindings);
    
    // Try to fetch a row
    int fetch = mysql_stmt_fetch(check_statement);
    // If fetch failed, auction doesn't exist
    if (fetch != 0)
    {
        send_html_header();
        cout << "<p>Auction not found.</p>";
        mysql_stmt_close(check_statement);
        return false; // Validation failed
    }
    
    // Close the statement (we got what we needed)
    mysql_stmt_close(check_statement);
    
    // Check if user is trying to bid on their own auction (not allowed!)
    if (seller_id == bidder_user_id)
    {
        send_html_header();
        cout << "<p>You cannot bid on your own item.</p>";
        return false; // Validation failed
    }
    
    // All checks passed
    return true;
}

// This function gets the current highest bid for an auction
// Returns either the max bid amount or the starting price (whichever is higher)
double get_current_top_bid(DatabaseConnection& database, unsigned long long auction_id)
{
    // SQL uses GREATEST to return the higher of: max bid or starting price
    // IFNULL returns 0 if there are no bids yet
    const char* top_bid_sql = 
        "SELECT GREATEST(IFNULL(MAX(amount), 0), (SELECT start_price FROM auctions WHERE id = ?)) "
        "FROM bids WHERE auction_id = ?";
    
    // Initialize prepared statement
    MYSQL_STMT* top_bid_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(top_bid_statement, top_bid_sql, strlen(top_bid_sql));
    
    // Create bindings for the two auction_id parameters (used twice in the SQL)
    MYSQL_BIND parameter_bindings[2];
    for(int i = 0; i < 2; i++) {
        parameter_bindings[i].buffer_type = MYSQL_TYPE_LONGLONG;  // Big integer
        parameter_bindings[i].buffer = &auction_id;               // Both use auction_id
        parameter_bindings[i].is_null = nullptr;                  // Not null
        parameter_bindings[i].length = nullptr;                   // No length variable
        parameter_bindings[i].error = nullptr;                    // No error variable
    }
    
    // Bind the parameters
    mysql_stmt_bind_param(top_bid_statement, parameter_bindings);
    // Execute the query
    mysql_stmt_execute(top_bid_statement);
    
    // Variable to hold the result
    double top_bid_amount;
    
    // Create result binding
    MYSQL_BIND result_binding;
    result_binding.buffer_type = MYSQL_TYPE_DOUBLE;    // Decimal number
    result_binding.buffer = &top_bid_amount;           // Store here
    result_binding.is_null = nullptr;                  // Not null
    result_binding.length = nullptr;                   // No length variable
    result_binding.error = nullptr;                    // No error variable
    
    // Bind the result
    mysql_stmt_bind_result(top_bid_statement, &result_binding);
    // Fetch the single result row
    mysql_stmt_fetch(top_bid_statement);
    // Close the statement
    mysql_stmt_close(top_bid_statement);
    
    // Return the top bid amount
    return top_bid_amount;
}

// This function inserts a new bid into the database
// Returns true if successful, false if error
bool place_bid_in_database(DatabaseConnection& database,
                           unsigned long long auction_id,
                           unsigned long long bidder_user_id,
                           double bid_amount)
{
    // SQL to insert a new bid record
    const char* insert_bid_sql = 
        "INSERT INTO bids(auction_id, bidder_id, amount) VALUES(?, ?, ?)";
    
    // Initialize prepared statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL
    mysql_stmt_prepare(insert_statement, insert_bid_sql, strlen(insert_bid_sql));
    
    // Create bindings array (3 parameters)
    MYSQL_BIND parameter_bindings[3];
    // Initialize all bindings
    for(int i = 0; i < 3; i++) {
        parameter_bindings[i].buffer_type = MYSQL_TYPE_NULL;
        parameter_bindings[i].buffer = nullptr;
        parameter_bindings[i].buffer_length = 0;
        parameter_bindings[i].is_null = nullptr;
        parameter_bindings[i].length = nullptr;
        parameter_bindings[i].error = nullptr;
    }
    
    // Make a copy of auction_id (need non-const variable)
    unsigned long long auction_copy = auction_id;
    // Bind auction_id parameter (first ?)
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &auction_copy;
    
    // Make a copy of bidder_user_id
    unsigned long long bidder_copy = bidder_user_id;
    // Bind bidder_id parameter (second ?)
    parameter_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[1].buffer = &bidder_copy;
    
    // Bind amount parameter (third ?)
    parameter_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[2].buffer = &bid_amount;
    
    // Bind all parameters to the statement
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    // Execute the insert
    int exec = mysql_stmt_execute(insert_statement);
    // Check if successful (0 means success)
    bool insert_successful = (exec == 0);
    // Close the statement
    mysql_stmt_close(insert_statement);
    
    // Return whether insert succeeded
    return insert_successful;
}

// This function handles placing a bid on an auction
void handle_bid_action(DatabaseConnection& database,
                       unsigned long long bidder_user_id,
                       const map<string, string>& form_data)
{
    // Extract form fields
    string auction_id_str = "";
    string amount_str = "";
    // Check if fields exist and get them
    if(form_data.find("auction_id") != form_data.end()) {
        auction_id_str = form_data.at("auction_id");
    }
    if(form_data.find("amount") != form_data.end()) {
        amount_str = form_data.at("amount");
    }
    
    // Convert auction_id string to a number
    unsigned long long auction_id = strtoull(auction_id_str.c_str(), nullptr, 10);
    // Convert amount string to a double
    double bid_amount = stod(amount_str);
    
    // Validate that user can bid on this auction
    bool valid = validate_auction_for_bidding(database, auction_id, bidder_user_id);
    if (!valid)
    {
        return; // Error message already sent by validation function
    }
    
    // Get the current highest bid
    double current_top_bid = get_current_top_bid(database, auction_id);
    
    // Check if new bid is high enough (must be greater than current top)
    if (bid_amount <= current_top_bid)
    {
        send_html_header();
        cout << "<p>Bid must be greater than current top ($";
        // Format as money with 2 decimal places
        cout << fixed << setprecision(2) << current_top_bid;
        cout << ").</p>";
        return; // Exit function
    }
    
    // Bid amount is valid, try to place it
    send_html_header();
    
    // Try to insert the bid into database
    bool placed = place_bid_in_database(database, auction_id, bidder_user_id, bid_amount);
    if (!placed)
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

// This is the main function - where the program starts
int main()
{
    // Turn off C I/O synchronization for faster output
    ios::sync_with_stdio(false);
    
    // Create database connection (connects automatically)
    DatabaseConnection database;
    
    // Get the query string from environment (for AJAX requests)
    const char* qs = getenv("QUERY_STRING");
    string query_string = "";
    if(qs) {
        query_string = qs;
    }
    
    // Check if this is an AJAX request for auction options
    bool is_options_request = false;
    // Search for "action=options" in the query string
    for(int i = 0; i < query_string.size() - 13; i++) {
        if(query_string.substr(i, 14) == "action=options") {
            is_options_request = true;
            break; // Found it
        }
    }
    
    // If it's an options request, just return the options and exit
    if (is_options_request)
    {
        handle_auction_options_request(database);
        return 0; // Done
    }
    
    // For other requests, we need a valid user session
    unsigned long long current_user_id;
    
    // Check if user has a valid session
    bool has_session = get_user_from_session(database, current_user_id);
    if (!has_session)
    {
        // No valid session - show error message
        send_html_header();
        cout << "<div class='notice'>Session expired (5 minutes idle). ";
        cout << "<a href='auth.html'>Sign in</a>.</div>";
        return 0; // Exit
    }
    
    // Parse the POST form data
    auto form_data = parse_post_data();
    // Get the action field
    string requested_action = "";
    if(form_data.find("action") != form_data.end()) {
        requested_action = form_data["action"];
    }
    
    // Check if user wants to sell an item
    if (requested_action == "sell")
    {
        handle_sell_action(database, current_user_id, form_data);
        return 0; // Done
    }
    
    // Check if user wants to place a bid
    if (requested_action == "bid")
    {
        handle_bid_action(database, current_user_id, form_data);
        return 0; // Done
    }
    
    // Unknown action
    send_html_header();
    cout << "<p>Unsupported action.</p>";
    return 0; // Exit
}