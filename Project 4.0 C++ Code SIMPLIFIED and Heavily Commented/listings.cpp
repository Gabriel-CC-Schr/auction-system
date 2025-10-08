// *******
// listings.cpp
// *******


// This header includes common C++ features we need (iostream, string, etc.)
#include <bits/stdc++.h>
// This header lets us use MySQL database functions
#include <mysql/mysql.h>

// This line lets us use standard library features without typing "std::" every time
using namespace std;

// These constants store our database connection information
// DB_HOST is where the database server is located (localhost = this computer)
const string DB_HOST = "localhost";
// DB_USER is the username to log into the database
const string DB_USER = "allindragons";
// DB_PASS is the password for the database user
const string DB_PASS = "snogardnilla_002";
// DB_NAME is the specific database we want to connect to
const string DB_NAME = "cs370_section2_allindragons";

// This struct manages our database connection
// It automatically connects when created and disconnects when destroyed
struct DatabaseConnection
{
    // This pointer will hold our MySQL connection object
    MYSQL* mysql_connection;
    
    // Constructor - runs when we create a DatabaseConnection object
    DatabaseConnection()
    {
        // Initialize the pointer to null (nothing) first
        mysql_connection = nullptr;
        // Initialize the MySQL library and get a connection handle
        mysql_connection = mysql_init(nullptr);
        
        // Actually connect to the database server
        MYSQL* conn_result = mysql_real_connect(
            mysql_connection,      // The connection we initialized
            DB_HOST.c_str(),       // Server address as C-style string
            DB_USER.c_str(),       // Username as C-style string
            DB_PASS.c_str(),       // Password as C-style string
            DB_NAME.c_str(),       // Database name as C-style string
            0,                     // Port (0 = use default port)
            nullptr,               // Unix socket (nullptr = don't use)
            0                      // Client flags (0 = no special flags)
        );
        
        // We should check if connection worked but we're not handling errors here
    }
    
    // Destructor - runs automatically when the object is destroyed
    ~DatabaseConnection()
    {
        // Check if we have a valid connection
        if (mysql_connection != nullptr)
        {
            // Close the connection to free resources
            mysql_close(mysql_connection);
        }
    }
};

// This function gets the currently logged-in user's ID from their session
// Returns 0 if no one is logged in
unsigned long long get_current_user_id(DatabaseConnection& database)
{
    // TODO: This needs to check the session cookie and validate it
    // For now we just return 0 (not implemented yet)
    return 0;
}

// This function closes any auctions that have expired
// It updates auctions in the database from OPEN to CLOSED status
void close_expired_auctions(DatabaseConnection& database)
{
    // This SQL query updates all open auctions whose end_time has passed
    // NOW() gets the current date/time from the database
    const char* close_expired_sql = 
        "UPDATE auctions SET status='CLOSED' "
        "WHERE status='OPEN' AND end_time <= NOW()";
    
    // Execute the query directly (no need for prepared statement here)
    mysql_query(database.mysql_connection, close_expired_sql);
    
    // We should check if the query worked but we're not handling errors
}

// This is the main function where our program starts
int main()
{
    // Turn off synchronization with C I/O to make output faster
    ios::sync_with_stdio(false);
    
    // Send HTTP headers to tell browser this is HTML content
    cout << "Content-Type: text/html\r\n\r\n";
    // Include our CSS stylesheet so the page looks nice
    cout << "<link rel=\"stylesheet\" href=\"style.css\">";
    
    // Create a database connection (constructor connects automatically)
    DatabaseConnection database;
    
    // Get the ID of whoever is currently logged in (0 if not logged in)
    unsigned long long current_user_id = get_current_user_id(database);
    
    // Close any auctions that have expired before showing the list
    close_expired_auctions(database);
    
    // This is a big SQL query that gets all open auctions
    // It uses a subquery to calculate the current price (highest bid or starting price)
    // COALESCE returns the first non-null value
    const char* auction_list_sql = R"SQL(
        SELECT a.id, a.title, a.description, a.start_price, a.end_time, a.seller_id,
               COALESCE((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id = a.id), a.start_price) AS current_price
        FROM auctions a
        WHERE a.status = 'OPEN' AND a.end_time > NOW()
        ORDER BY a.end_time ASC
    )SQL";
    
    // Initialize a prepared statement for safety and efficiency
    MYSQL_STMT* auction_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL query
    mysql_stmt_prepare(auction_statement, auction_list_sql, strlen(auction_list_sql));
    // Execute the query to get results
    mysql_stmt_execute(auction_statement);
    
    // Declare variables to hold the data we get back from each row
    unsigned long long auction_id;           // The auction's ID number
    unsigned long long seller_id;            // Who is selling this item
    char auction_title_buffer[121];          // Buffer to hold auction title (max 120 chars + null)
    char auction_description_buffer[1025];   // Buffer to hold description (max 1024 chars + null)
    char end_time_buffer[20];                // Buffer to hold end time string
    unsigned long title_length;              // Actual length of title we receive
    unsigned long description_length;        // Actual length of description we receive
    unsigned long end_time_length;           // Actual length of end time we receive
    double starting_price;                   // The starting price of the auction
    double current_price;                    // The current highest bid (or starting price)
    
    // Create an array of bindings to tell MySQL where to put the data
    MYSQL_BIND result_bindings[7];  // We have 7 columns in our SELECT query
    // Zero out all the binding structures first (good practice)
    for(int i = 0; i < 7; i++) {
        result_bindings[i].buffer_type = MYSQL_TYPE_NULL;    // Set to null initially
        result_bindings[i].buffer = nullptr;                 // No buffer yet
        result_bindings[i].buffer_length = 0;                // No length yet
        result_bindings[i].is_null = nullptr;                // Not null indicator
        result_bindings[i].length = nullptr;                 // No length variable
        result_bindings[i].error = nullptr;                  // No error variable
    }
    
    // Bind the auction ID column (first column)
    result_bindings[0].buffer_type = MYSQL_TYPE_LONGLONG;    // It's a big integer
    result_bindings[0].buffer = &auction_id;                 // Store it here
    
    // Bind the title column (second column)
    result_bindings[1].buffer_type = MYSQL_TYPE_STRING;      // It's a string
    result_bindings[1].buffer = auction_title_buffer;        // Store it in this buffer
    result_bindings[1].buffer_length = 120;                  // Maximum size of buffer
    result_bindings[1].length = &title_length;               // Store actual length here
    
    // Bind the description column (third column)
    result_bindings[2].buffer_type = MYSQL_TYPE_STRING;      // It's a string
    result_bindings[2].buffer = auction_description_buffer;  // Store it in this buffer
    result_bindings[2].buffer_length = 1024;                 // Maximum size of buffer
    result_bindings[2].length = &description_length;         // Store actual length here
    
    // Bind the starting price column (fourth column)
    result_bindings[3].buffer_type = MYSQL_TYPE_DOUBLE;      // It's a decimal number
    result_bindings[3].buffer = &starting_price;             // Store it here
    
    // Bind the end time column (fifth column)
    result_bindings[4].buffer_type = MYSQL_TYPE_STRING;      // It's a string
    result_bindings[4].buffer = end_time_buffer;             // Store it in this buffer
    result_bindings[4].buffer_length = 20;                   // Maximum size of buffer
    result_bindings[4].length = &end_time_length;            // Store actual length here
    
    // Bind the seller ID column (sixth column)
    result_bindings[5].buffer_type = MYSQL_TYPE_LONGLONG;    // It's a big integer
    result_bindings[5].buffer = &seller_id;                  // Store it here
    
    // Bind the current price column (seventh column)
    result_bindings[6].buffer_type = MYSQL_TYPE_DOUBLE;      // It's a decimal number
    result_bindings[6].buffer = &current_price;              // Store it here
    
    // Bind all the result columns to our variables
    mysql_stmt_bind_result(auction_statement, result_bindings);
    
    // Start generating the HTML output
    cout << "<h2>Open Auctions</h2>";
    // Add a note about the sort order
    cout << "<div class='small'>Ending soonest first.</div>";
    // Start a card container and list
    cout << "<div class='card'><ul class='list'>";
    
    // Set the output format for decimal numbers (fixed point, 2 decimal places)
    cout.setf(std::ios::fixed);
    cout << setprecision(2);
    
    // Try to fetch the first row of results
    int fetch_result = mysql_stmt_fetch(auction_statement);
    // Keep looping as long as we get rows (0 means success)
    while (fetch_result == 0)
    {
        // Convert the title buffer to a proper C++ string
        string auction_title = "";  // Start with empty string
        // Copy each character from the buffer
        for(int i = 0; i < title_length; i++) {
            auction_title += auction_title_buffer[i];
        }
        
        // Convert the description buffer to a proper C++ string
        string auction_description = "";  // Start with empty string
        // Copy each character from the buffer
        for(int i = 0; i < description_length; i++) {
            auction_description += auction_description_buffer[i];
        }
        
        // Convert the end time buffer to a proper C++ string
        string end_time_string = "";  // Start with empty string
        // Copy each character from the buffer
        for(int i = 0; i < end_time_length; i++) {
            end_time_string += end_time_buffer[i];
        }
        
        // Start a list item for this auction
        cout << "<li>";
        // Start a span for the auction info
        cout << "<span>";
        // Output the title in bold
        cout << "<strong>" << auction_title << "</strong><br>";
        // Output the description in small text
        cout << "<span class='small'>" << auction_description << "</span><br>";
        // Output the end time in small text
        cout << "<span class='small'>Ends " << end_time_string << "</span>";
        // Close the info span
        cout << "</span>";
        
        // Start a span for the price and bid link
        cout << "<span>";
        // Output the current price with dollar sign
        cout << "$" << current_price;
        
        // Check if user is logged in (not 0)
        bool is_logged_in = (current_user_id != 0);
        // Check if user is not the seller
        bool is_not_seller = (current_user_id != seller_id);
        // Only show bid link if logged in AND not the seller
        if (is_logged_in && is_not_seller)
        {
            // Add a link to the bidding page
            cout << " <a href='trade.html'>Bid</a>";
        }
        
        // Close the price span
        cout << "</span>";
        // Close the list item
        cout << "</li>";
        
        // Try to fetch the next row
        fetch_result = mysql_stmt_fetch(auction_statement);
    }
    
    // Close the HTML list and div
    cout << "</ul></div>";
    
    // Close the prepared statement to free memory
    mysql_stmt_close(auction_statement);
    
    // Exit the program successfully
    return 0;
}