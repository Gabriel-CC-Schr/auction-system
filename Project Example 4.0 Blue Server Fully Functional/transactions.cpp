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
 * Gets current user ID from session, returns 0 if not logged in
 */
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    unsigned long long user_id = 0;
    get_user_from_session(database, user_id);
    return user_id;
}

/**
 * Displays active bids where user is currently participating
 * Shows if user has been outbid and provides link to increase bid
 */
void display_active_bids_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    cout << "<div class='card'><h3>Active Bids</h3>";
    
    // SQL query to get user's active bids with current auction status
    // Shows user's bid amount vs current top bid to determine if outbid
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
    
    // Prepare the SQL statement
    MYSQL_STMT* active_bids_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(active_bids_statement, active_bids_sql, strlen(active_bids_sql));
    
    // Set up parameter bindings (user ID used twice)
    MYSQL_BIND parameter_bindings[2]{};
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &current_user_id;
    parameter_bindings[1] = parameter_bindings[0]; // Same user ID for both parameters
    
    mysql_stmt_bind_param(active_bids_statement, parameter_bindings);
    mysql_stmt_execute(active_bids_statement);
    
    // Variables to receive query results
    char auction_title_buffer[121];
    unsigned long title_length;
    double current_top_bid;
    double my_bid_amount;
    
    // Set up result bindings
    MYSQL_BIND result_bindings[3]{};
    
    // Bind auction title
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Bind current top bid amount
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &current_top_bid;
    
    // Bind user's bid amount
    result_bindings[2].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[2].buffer = &my_bid_amount;
    
    mysql_stmt_bind_result(active_bids_statement, result_bindings);
    
    // Start HTML list
    cout << "<ul class='list'>";
    
    // Set decimal formatting for currency display
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Display each active bid
    while (mysql_stmt_fetch(active_bids_statement) == 0)
    {
        // Convert title to C++ string
        string auction_title(auction_title_buffer, title_length);
        
        // Check if user has been outbid
        bool user_is_outbid = (my_bid_amount < current_top_bid);
        
        // Display bid information
        cout << "<li>";
        cout << "<span>";
        cout << "<strong>" << auction_title << "</strong>";
        
        // Show outbid warning if applicable
        if (user_is_outbid)
        {
            cout << " <span class='notice'>Outbid</span>";
        }
        
        cout << "</span>";
        cout << "<span>";
        cout << "$" << current_top_bid << " <a href='trade.html'>Increase bid</a>";
        cout << "</span>";
        cout << "</li>";
    }
    
    cout << "</ul>";
    mysql_stmt_close(active_bids_statement);
    cout << "</div>";
}

/**
 * Displays closed auctions where user participated but didn't win
 * Shows the winning bid amount for reference
 */
void display_lost_auctions_section(DatabaseConnection& database, unsigned long long current_user_id)
{
    cout << "<div class='card'><h3>Didn't Win</h3>";
    
    // SQL query to find closed auctions where user bid but didn't win
    // Shows only auctions where user placed bids but someone else won
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
    
    // Prepare the SQL statement
    MYSQL_STMT* lost_auctions_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(lost_auctions_statement, lost_auctions_sql, strlen(lost_auctions_sql));
    
    // Set up parameter bindings (user ID used twice)
    MYSQL_BIND parameter_bindings[2]{};
    parameter_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    parameter_bindings[0].buffer = &current_user_id;
    parameter_bindings[1] = parameter_bindings[0]; // Same user ID for both parameters
    
    mysql_stmt_bind_param(lost_auctions_statement, parameter_bindings);
    mysql_stmt_execute(lost_auctions_statement);
    
    // Variables to receive query results
    char auction_title_buffer[121];
    unsigned long title_length;
    double winning_amount;
    
    // Set up result bindings
    MYSQL_BIND result_bindings[2]{};
    
    // Bind auction title
    result_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[0].buffer = auction_title_buffer;
    result_bindings[0].buffer_length = 120;
    result_bindings[0].length = &title_length;
    
    // Bind winning bid amount
    result_bindings[1].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[1].buffer = &winning_amount;
    
    mysql_stmt_bind_result(lost_auctions_statement, result_bindings);
    
    // Start HTML list
    cout << "<ul class='list'>";
    
    // Set decimal formatting for currency display
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Display each lost auction
    while (mysql_stmt_fetch(lost_auctions_statement) == 0)
    {
        // Convert title to C++ string
        string auction_title(auction_title_buffer, title_length);
        
        // Display auction information
        cout << "<li>";
        cout << "<span><strong>" << auction_title << "</strong></span>";
        cout << "<span>$" << winning_amount << "</span>";
        cout << "</li>";
    }
    
    cout << "</ul>";
    mysql_stmt_close(lost_auctions_statement);
    cout << "</div>";
}

/**
 * Main CGI entry point
 * Displays user's transaction history: active bids and lost auctions
 */
int main()
{
    // Optimize I/O performance
    ios::sync_with_stdio(false);
    
    // Send HTTP headers
    send_html_header();
    
    // Initialize database connection
    DatabaseConnection database;
    
    // Get current logged-in user ID
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Redirect to login if no valid session
    if (current_user_id == 0)
    {
        cout << "Status: 303 See Other\r\n"
             << "Location: auth.html\r\n"
             << "Content-Type: text/html\r\n\r\n"
             << "Session expired. Please log in.";
        return 0;
    }
    
    // Display active bids section
    display_active_bids_section(database, current_user_id);
    
    // Display lost auctions section
    display_lost_auctions_section(database, current_user_id);
    
    return 0;
}