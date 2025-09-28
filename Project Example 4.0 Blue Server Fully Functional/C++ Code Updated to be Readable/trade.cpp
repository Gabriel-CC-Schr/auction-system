// Pull in common STL headers (GCC convenience)
#include <bits/stdc++.h>
// MariaDB/MySQL C API header
#include <mysql/mysql.h>

// Bring std symbols into the global namespace for concise code
using namespace std;

// Database connection constants (consistent with other files)
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

/**
 * RAII wrapper for MySQL database connection
 * Opens connection in constructor, closes in destructor
 * (Consistent with other files)
 */
struct DatabaseConnection
{
    MYSQL* mysql_connection = nullptr;
    
    // Constructor: establish database connection
    DatabaseConnection()
    {
        mysql_connection = mysql_init(nullptr);
        
        mysql_real_connect(
            mysql_connection,
            DB_HOST.c_str(),
            DB_USER.c_str(),
            DB_PASS.c_str(),
            DB_NAME.c_str(),
            0,                  // port (0 = default)
            nullptr,            // unix_socket
            0                   // client_flag
        );
    }
    
    // Destructor: close database connection
    ~DatabaseConnection()
    {
        if (mysql_connection != nullptr)
        {
            mysql_close(mysql_connection);
        }
    }
};

/**
 * Sends standard HTML header for CGI responses
 */
void send_html_header()
{
    cout << "Content-Type: text/html\r\n\r\n";
}

/**
 * URL-decodes application/x-www-form-urlencoded strings
 */
string url_decode(const string &encoded_string)
{
    string output_string;
    output_string.reserve(encoded_string.size());
    
    for (size_t current_index = 0; current_index < encoded_string.size(); ++current_index)
    {
        if (encoded_string[current_index] == '+')
        {
            output_string += ' ';
        }
        else if (encoded_string[current_index] == '%' && 
                 current_index + 2 < encoded_string.size())
        {
            string hex_string = encoded_string.substr(current_index + 1, 2);
            char decoded_char = static_cast<char>(strtol(hex_string.c_str(), nullptr, 16));
            output_string += decoded_char;
            current_index += 2;
        }
        else
        {
            output_string += encoded_string[current_index];
        }
    }
    
    return output_string;
}

/**
 * Reads POST body and parses form data
 */
map<string, string> parse_post_data()
{
    const char* content_length_env = getenv("CONTENT_LENGTH");
    string content_length_string = content_length_env ? content_length_env : "0";
    int content_length = atoi(content_length_string.c_str());
    
    string post_body(max(0, content_length), '\0');
    if (content_length > 0)
    {
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    map<string, string> form_data;
    size_t current_position = 0;
    
    while (current_position < post_body.size())
    {
        size_t equals_position = post_body.find('=', current_position);
        if (equals_position == string::npos) break;
        
        size_t ampersand_position = post_body.find('&', equals_position);
        
        string raw_key = post_body.substr(current_position, equals_position - current_position);
        string decoded_key = url_decode(raw_key);
        
        size_t value_start = equals_position + 1;
        size_t value_end = (ampersand_position == string::npos) ? post_body.size() : ampersand_position;
        string raw_value = post_body.substr(value_start, value_end - value_start);
        string decoded_value = url_decode(raw_value);
        
        form_data[decoded_key] = decoded_value;
        
        if (ampersand_position == string::npos) break;
        current_position = ampersand_position + 1;
    }
    
    return form_data;
}

/**
 * Extracts session token from HTTP_COOKIE environment variable
 */
string get_session_token_from_cookie()
{
    const char* cookie_header = getenv("HTTP_COOKIE");
    if (!cookie_header) return "";
    
    string cookies(cookie_header);
    size_t session_pos = cookies.find("session=");
    if (session_pos == string::npos) return "";
    
    size_t start_pos = session_pos + 8;
    size_t end_pos = cookies.find(';', start_pos);
    if (end_pos == string::npos) end_pos = cookies.length();
    
    return cookies.substr(start_pos, end_pos - start_pos);
}

/**
 * Validates session and returns user ID
 */
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    string session_token = get_session_token_from_cookie();
    if (session_token.empty()) return false;
    
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    if (mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql)))
    {
        mysql_stmt_close(session_statement);
        return false;
    }
    
    MYSQL_BIND param_binding{};
    param_binding.buffer_type = MYSQL_TYPE_STRING;
    param_binding.buffer = (void*)session_token.c_str();
    param_binding.buffer_length = session_token.size();
    
    mysql_stmt_bind_param(session_statement, &param_binding);
    mysql_stmt_execute(session_statement);
    
    MYSQL_BIND result_binding{};
    result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    result_binding.buffer = &out_user_id;
    
    mysql_stmt_bind_result(session_statement, &result_binding);
    bool session_valid = (mysql_stmt_fetch(session_statement) == 0);
    mysql_stmt_close(session_statement);
    
    if (session_valid)
    {
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        mysql_stmt_bind_param(update_stmt, &param_binding);
        mysql_stmt_execute(update_stmt);
        mysql_stmt_close(update_stmt);
    }
    
    return session_valid;
}

/**
 * Generates HTML <option> elements for all open auctions
 * Used by AJAX/iframe requests to populate auction selection dropdowns
 */
void handle_auction_options_request(DatabaseConnection& database)
{
    // SQL query to get all open auctions for dropdown options
    const char* auction_options_sql = 
        "SELECT id, title FROM auctions "
        "WHERE status = 'OPEN' AND end_time > NOW() "
        "ORDER BY end_time ASC";
    
    // Prepare the SQL statement
    MYSQL_STMT* options_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(options_statement, auction_options_sql, strlen(auction_options_sql));
    mysql_stmt_execute(options_statement);
    
    // Variables to receive query results
    unsigned long long auction_id;
    char auction_title_buffer[121];
    unsigned long title_length;
    
    // Set up result bindings
    MYSQL_BIND result_bindings[2]{};
    
    // Bind auction ID
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &auction_id;
    
    // Bind auction title
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = auction_title_buffer;
    result_bindings[1].buffer_length = 120;
    result_bindings[1].length = &title_length;
    
    mysql_stmt_bind_result(options_statement, result_bindings);
    
    // Send HTTP headers
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Generate HTML option elements
    while (mysql_stmt_fetch(options_statement) == 0)
    {
        string auction_title(auction_title_buffer, title_length);
        cout << "<option value='" << auction_id << "'>" << auction_title << "</option>";
    }
    
    mysql_stmt_close(options_statement);
}

/**
 * Handles the "sell" action - creates a new auction listing
 * Auction duration is set to 7 days (168 hours) from start time
 */
void handle_sell_action(DatabaseConnection& database, 
                       unsigned long long seller_user_id,
                       const map<string, string>& form_data)
{
    // Extract form fields
    string auction_title = form_data.at("title");
    string auction_description = form_data.at("description");
    string start_price_string = form_data.at("start_price");
    string start_time = form_data.at("start_time");
    
    // Validate required fields
    if (auction_title.empty() || auction_description.empty() || 
        start_price_string.empty() || start_time.empty())
    {
        send_html_header();
        cout << "<p>Missing fields.</p>";
        return;
    }
    
    // Convert start price to double
    double starting_price = stod(start_price_string);
    
    // SQL query to insert new auction with 7-day duration
    const char* insert_auction_sql = 
        "INSERT INTO auctions(seller_id, title, description, start_price, start_time, end_time, status) "
        "VALUES(?, ?, ?, ?, ?, DATE_ADD(?, INTERVAL 168 HOUR), 'OPEN')";
    
    // Prepare the SQL statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(insert_statement, insert_auction_sql, strlen(insert_auction_sql));
    
    // Set up parameter bindings
    MYSQL_BIND parameter_bindings[6]{};
    
    // Bind seller ID
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &seller_user_id;
    
    // Bind auction title
    parameter_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[1].buffer = (void*)auction_title.c_str();
    parameter_bindings[1].buffer_length = auction_title.size();
    
    // Bind auction description
    parameter_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[2].buffer = (void*)auction_description.c_str();
    parameter_bindings[2].buffer_length = auction_description.size();
    
    // Bind starting price
    parameter_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[3].buffer = &starting_price;
    
    // Bind start time (for start_time field)
    parameter_bindings[4].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[4].buffer = (void*)start_time.c_str();
    parameter_bindings[4].buffer_length = start_time.size();
    
    // Bind start time again (for end_time calculation)
    parameter_bindings[5].buffer_type = MYSQL_TYPE_STRING;
    parameter_bindings[5].buffer = (void*)start_time.c_str();
    parameter_bindings[5].buffer_length = start_time.size();
    
    // Execute the insert statement
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    
    send_html_header();
    
    if (mysql_stmt_execute(insert_statement) != 0)
    {
        cout << "<p>Error listing item.</p>";
    }
    else
    {
        cout << "<p>Item listed for 7 days. <a href='listings.html'>View listings</a></p>";
    }
    
    mysql_stmt_close(insert_statement);
}

/**
 * Validates that an auction exists and user can bid on it
 * Returns true if validation passes, false otherwise
 */
bool validate_auction_for_bidding(DatabaseConnection& database,
                                  unsigned long long auction_id,
                                  unsigned long long bidder_user_id)
{
    // SQL query to check auction status and seller
    const char* auction_check_sql = 
        "SELECT seller_id, status, end_time FROM auctions WHERE id = ?";
    
    // Prepare the SQL statement
    MYSQL_STMT* check_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(check_statement, auction_check_sql, strlen(auction_check_sql));
    
    // Bind auction ID parameter
    MYSQL_BIND parameter_binding{};
    parameter_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_binding.buffer = &auction_id;
    mysql_stmt_bind_param(check_statement, &parameter_binding);
    
    mysql_stmt_execute(check_statement);
    
    // Variables to receive results
    unsigned long long seller_id;
    char status_buffer[6];
    char end_time_buffer[20];
    unsigned long status_length;
    unsigned long end_time_length;
    
    // Set up result bindings
    MYSQL_BIND result_bindings[3]{};
    
    // Bind seller ID
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &seller_id;
    
    // Bind auction status
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = status_buffer;
    result_bindings[1].buffer_length = 6;
    result_bindings[1].length = &status_length;
    
    // Bind end time
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[2].buffer = end_time_buffer;
    result_bindings[2].buffer_length = 20;
    result_bindings[2].length = &end_time_length;
    
    mysql_stmt_bind_result(check_statement, result_bindings);
    
    // Check if auction exists
    if (mysql_stmt_fetch(check_statement) != 0)
    {
        send_html_header();
        cout << "<p>Auction not found.</p>";
        mysql_stmt_close(check_statement);
        return false;
    }
    
    mysql_stmt_close(check_statement);
    
    // Check if user is trying to bid on their own auction
    if (seller_id == bidder_user_id)
    {
        send_html_header();
        cout << "<p>You cannot bid on your own item.</p>";
        return false;
    }
    
    return true;
}

/**
 * Gets the current highest bid amount for an auction
 * Returns the higher of: maximum bid amount or starting price
 */
double get_current_top_bid(DatabaseConnection& database, unsigned long long auction_id)
{
    // SQL query to get the current top bid (max of bids or starting price)
    const char* top_bid_sql = 
        "SELECT GREATEST(IFNULL(MAX(amount), 0), (SELECT start_price FROM auctions WHERE id = ?)) "
        "FROM bids WHERE auction_id = ?";
    
    // Prepare the SQL statement
    MYSQL_STMT* top_bid_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(top_bid_statement, top_bid_sql, strlen(top_bid_sql));
    
    // Set up parameter bindings (auction_id used twice)
    MYSQL_BIND parameter_bindings[2]{};
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &auction_id;
    parameter_bindings[1] = parameter_bindings[0]; // Same auction_id for both parameters
    
    mysql_stmt_bind_param(top_bid_statement, parameter_bindings);
    mysql_stmt_execute(top_bid_statement);
    
    // Variable to receive the top bid amount
    double top_bid_amount;
    
    // Set up result binding
    MYSQL_BIND result_binding{};
    result_binding.buffer_type = MYSQL_TYPE_DOUBLE;
    result_binding.buffer = &top_bid_amount;
    
    mysql_stmt_bind_result(top_bid_statement, &result_binding);
    mysql_stmt_fetch(top_bid_statement);
    mysql_stmt_close(top_bid_statement);
    
    return top_bid_amount;
}

/**
 * Inserts a new bid into the database
 * Returns true if successful, false if error occurred
 */
bool place_bid_in_database(DatabaseConnection& database,
                           unsigned long long auction_id,
                           unsigned long long bidder_user_id,
                           double bid_amount)
{
    // SQL query to insert new bid
    const char* insert_bid_sql = 
        "INSERT INTO bids(auction_id, bidder_id, amount) VALUES(?, ?, ?)";
    
    // Prepare the SQL statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(insert_statement, insert_bid_sql, strlen(insert_bid_sql));
    
    // Set up parameter bindings
    MYSQL_BIND parameter_bindings[3]{};
    
    // Bind auction ID
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &auction_id;
    
    // Bind bidder ID
    parameter_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[1].buffer = &bidder_user_id;
    
    // Bind bid amount
    parameter_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    parameter_bindings[2].buffer = &bid_amount;
    
    // Execute the insert
    mysql_stmt_bind_param(insert_statement, parameter_bindings);
    bool insert_successful = (mysql_stmt_execute(insert_statement) == 0);
    mysql_stmt_close(insert_statement);
    
    return insert_successful;
}

/**
 * Handles the "bid" action - places a bid on an auction
 * Enforces business rules: no self-bidding, bid must exceed current top
 */
void handle_bid_action(DatabaseConnection& database,
                       unsigned long long bidder_user_id,
                       const map<string, string>& form_data)
{
    // Extract form data
    unsigned long long auction_id = strtoull(form_data.at("auction_id").c_str(), nullptr, 10);
    double bid_amount = stod(form_data.at("amount"));
    
    // Validate auction and user eligibility
    if (!validate_auction_for_bidding(database, auction_id, bidder_user_id))
    {
        return; // Error message already sent by validation function
    }
    
    // Get current top bid
    double current_top_bid = get_current_top_bid(database, auction_id);
    
    // Ensure new bid is higher than current top
    if (bid_amount <= current_top_bid)
    {
        send_html_header();
        cout << "<p>Bid must be greater than current top ($" 
             << fixed << setprecision(2) << current_top_bid << ").</p>";
        return;
    }
    
    // Attempt to place the bid
    send_html_header();
    
    if (!place_bid_in_database(database, auction_id, bidder_user_id, bid_amount))
    {
        cout << "<p>Error placing bid.</p>";
    }
    else
    {
        cout << "<p>Bid placed! <a href='transactions.html'>View my bids</a></p>";
    }
}

/**
 * Main CGI entry point
 * Handles trading operations: sell items, bid on auctions, get auction options
 */
int main()
{
    // Optimize I/O performance
    ios::sync_with_stdio(false);
    
    // Initialize database connection
    DatabaseConnection database;
    
    // Check if this is an AJAX request for auction options
    string query_string = getenv("QUERY_STRING") ? getenv("QUERY_STRING") : "";
    
    if (query_string.find("action=options") != string::npos)
    {
        handle_auction_options_request(database);
        return 0;
    }
    
    // Get session token from cookie and validate user session
    unsigned long long current_user_id;
    
    // Require valid session (idle <= 5 minutes) for sell/bid operations
    if (!get_user_from_session(database, current_user_id))
    {
        send_html_header();
        cout << "<div class='notice'>Session expired (5 minutes idle). "
             << "<a href='auth.html'>Sign in</a>.</div>";
        return 0;
    }
    
    // Parse POST form data
    auto form_data = parse_post_data();
    string requested_action = form_data["action"];
    
    // Handle sell action
    if (requested_action == "sell")
    {
        handle_sell_action(database, current_user_id, form_data);
        return 0;
    }
    
    // Handle bid action
    if (requested_action == "bid")
    {
        handle_bid_action(database, current_user_id, form_data);
        return 0;
    }
    
    // Handle unknown actions
    send_html_header();
    cout << "<p>Unsupported action.</p>";
    return 0;
}