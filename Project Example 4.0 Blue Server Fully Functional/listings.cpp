// Single-header convenience (GCC): includes common STL headers
#include <bits/stdc++.h>
// MariaDB/MySQL C API header
#include <mysql/mysql.h>

// Bring std symbols into the global namespace for concise code
using namespace std;

// Database connection constants (should match auth.cpp)
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

/**
 * RAII wrapper for MySQL database connection
 * Opens connection in constructor, closes in destructor
 * (Consistent with auth.cpp DatabaseConnection)
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
 * Gets the current logged-in user's ID from session cookie
 * Returns 0 if no valid session found
 * (This function should be defined elsewhere or imported)
 */
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    // TODO: This function needs to be implemented
    // Should check session cookie and return user ID
    // Returning 0 for now as placeholder
    return 0;
}

/**
 * Performs maintenance by closing expired auctions
 * Updates any auctions past their end_time to CLOSED status
 */
void close_expired_auctions(DatabaseConnection& database)
{
    const char* close_expired_sql = 
        "UPDATE auctions SET status='CLOSED' "
        "WHERE status='OPEN' AND end_time <= NOW()";
    
    mysql_query(database.mysql_connection, close_expired_sql);
}

/**
 * Main CGI entry point
 * Displays HTML page showing all open auctions with bid links for non-owners
 */
int main()
{
    // Optimize I/O performance
    ios::sync_with_stdio(false);
    
    // Send HTTP headers
    cout << "Content-Type: text/html\r\n\r\n";
    cout << "<link rel=\"stylesheet\" href=\"style.css\">";
    
    // Initialize database connection
    DatabaseConnection database;
    
    // Get current logged-in user ID (0 if not logged in)
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Perform maintenance: close any expired auctions
    close_expired_auctions(database);
    
    // SQL query to get all open auctions with current prices
    // Current price is the maximum bid amount, or starting price if no bids
    const char* auction_list_sql = R"SQL(
        SELECT a.id, a.title, a.description, a.start_price, a.end_time, a.seller_id,
               COALESCE((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id = a.id), a.start_price) AS current_price
        FROM auctions a
        WHERE a.status = 'OPEN' AND a.end_time > NOW()
        ORDER BY a.end_time ASC
    )SQL";
    
    // Prepare the SQL statement
    MYSQL_STMT* auction_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(auction_statement, auction_list_sql, strlen(auction_list_sql));
    mysql_stmt_execute(auction_statement);
    
    // Declare variables to receive query results
    unsigned long long auction_id;
    unsigned long long seller_id;
    char auction_title_buffer[121];
    char auction_description_buffer[1025];
    char end_time_buffer[20];
    unsigned long title_length;
    unsigned long description_length;
    unsigned long end_time_length;
    double starting_price;
    double current_price;
    
    // Set up result bindings
    MYSQL_BIND result_bindings[7]{};
    
    // Bind auction ID
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[0].buffer = &auction_id;
    
    // Bind auction title
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[1].buffer = auction_title_buffer;
    result_bindings[1].buffer_length = 120;
    result_bindings[1].length = &title_length;
    
    // Bind auction description
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[2].buffer = auction_description_buffer;
    result_bindings[2].buffer_length = 1024;
    result_bindings[2].length = &description_length;
    
    // Bind starting price
    result_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[3].buffer = &starting_price;
    
    // Bind end time
    result_bindings[4].buffer_type = MYSQL_TYPE_STRING;
    result_bindings[4].buffer = end_time_buffer;
    result_bindings[4].buffer_length = 20;
    result_bindings[4].length = &end_time_length;
    
    // Bind seller ID
    result_bindings[5].buffer_type = MYSQL_TYPE_LONGLONG;
    result_bindings[5].buffer = &seller_id;
    
    // Bind current price
    result_bindings[6].buffer_type = MYSQL_TYPE_DOUBLE;
    result_bindings[6].buffer = &current_price;
    
    // Bind the result structure to the statement
    mysql_stmt_bind_result(auction_statement, result_bindings);
    
    // Start rendering the HTML page
    cout << "<h2>Open Auctions</h2>";
    cout << "<div class='small'>Ending soonest first.</div>";
    cout << "<div class='card'><ul class='list'>";
    
    // Set decimal formatting for currency display
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Iterate through all auction results
    while (mysql_stmt_fetch(auction_statement) == 0)
    {
        // Convert C-style strings to C++ strings with proper lengths
        string auction_title(auction_title_buffer, title_length);
        string auction_description(auction_description_buffer, description_length);
        string end_time_string(end_time_buffer, end_time_length);
        
        // Start auction list item
        cout << "<li>";
        cout << "<span>";
        cout << "<strong>" << auction_title << "</strong><br>";
        cout << "<span class='small'>" << auction_description << "</span><br>";
        cout << "<span class='small'>Ends " << end_time_string << "</span>";
        cout << "</span>";
        
        // Price and bid section
        cout << "<span>";
        cout << "$" << current_price;
        
        // Show bid link only if user is logged in and is not the seller
        if (current_user_id != 0 && current_user_id != seller_id)
        {
            cout << " <a href='trade.html'>Bid</a>";
        }
        
        cout << "</span>";
        cout << "</li>";
    }
    
    // Close the HTML list structure
    cout << "</ul></div>";
    
    // Clean up the prepared statement
    mysql_stmt_close(auction_statement);
    
    return 0;
}