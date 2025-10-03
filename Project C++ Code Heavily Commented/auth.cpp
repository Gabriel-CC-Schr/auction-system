// This line includes ALL standard C++ libraries at once (iostream, string, vector, etc.)
// It's a GCC compiler shortcut - saves time but only works with GCC/G++
#include <bits/stdc++.h>

// This includes the MySQL C library so we can talk to our database
// We need this to store and retrieve user accounts
#include <mysql/mysql.h>

// This line lets us use standard C++ functions without typing "std::" every time
// Example: We can write "string" instead of "std::string"
using namespace std;

// These are CONSTANTS - values that never change during program execution
// They store our database connection information
const string DB_HOST = "localhost";                      // Database is on the same computer as our program
const string DB_USER = "allindragons";   				 // Username to connect to MySQL
const string DB_PASS = "snogardnilla_002";               // Password to connect to MySQL
const string DB_NAME = "cs370_section2_allindragons";   // Name of the database we want to use

// PEPPER is a secret value we add to passwords before hashing
// It's like adding extra security - even if someone steals the database, they need this too
const string PEPPER = "site_pepper_2025_minimal_demo";

/**
 * FUNCTION: url_decode
 * PURPOSE: Convert URL-encoded text back to normal readable text
 * EXAMPLE: "Hello%20World" becomes "Hello World"
 * WHY: Web browsers encode special characters in URLs/forms, we need to decode them
 */
string url_decode(const string &encoded_string)
{
    // Create an empty string to store our decoded result
    string output_string;
    
    // Reserve memory space equal to input size (optimization - prevents reallocation)
    output_string.reserve(encoded_string.size());
    
    // Loop through every character in the encoded string
    // current_index starts at 0 and increases by 1 each time
    for (size_t current_index = 0; current_index < encoded_string.size(); ++current_index)
    {
        // If we find a plus sign, it represents a space
        if (encoded_string[current_index] == '+')
        {
            // Add a space character to our output
            output_string += ' ';
        }
        // If we find a percent sign followed by 2 more characters
        // Example: %20 means a space, %2B means a plus sign
        else if (encoded_string[current_index] == '%' && 
                 current_index + 2 < encoded_string.size())
        {
            // Extract the two characters after the % (these are hex digits)
            string hex_string = encoded_string.substr(current_index + 1, 2);
            
            // Convert the hex string to a number, then to a character
            // Example: "20" in hex = 32 in decimal = space character
            char decoded_char = static_cast<char>(strtol(hex_string.c_str(), nullptr, 16));
            
            // Add this decoded character to our output
            output_string += decoded_char;
            
            // Skip the next 2 characters since we already processed them
            current_index += 2;
        }
        else
        {
            // This is a regular character, just copy it as-is
            output_string += encoded_string[current_index];
        }
    }
    
    // Return the fully decoded string
    return output_string;
}

/**
 * FUNCTION: parse_post_data
 * PURPOSE: Read form data submitted by the user and convert it to key-value pairs
 * EXAMPLE: "email=user@test.com&password=secret123" becomes a map with two entries
 * WHY: Forms send data as one long string, we need to break it into usable pieces
 */
map<string, string> parse_post_data()
{
    // Get the CONTENT_LENGTH environment variable (tells us how much data was sent)
    // getenv returns NULL if the variable doesn't exist, so we check for that
    const char* content_length_env = getenv("CONTENT_LENGTH");
    
    // If CONTENT_LENGTH exists, use it; otherwise use "0"
    string content_length_string = content_length_env ? content_length_env : "0";
    
    // Convert the string to an integer (number of bytes to read)
    int content_length = atoi(content_length_string.c_str());
    
    // Create a string with enough space to hold all the POST data
    // max(0, content_length) ensures we don't create a negative-sized string
    string post_body(max(0, content_length), '\0');
    
    // If there's actually data to read (length > 0)
    if (content_length > 0)
    {
        // Read the exact number of bytes from standard input (stdin)
        // The web server sends form data through stdin
        // &post_body[0] gets a pointer to the first character in the string
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    // Create a map (dictionary) to store our key-value pairs
    // Example: form_data["email"] = "user@test.com"
    map<string, string> form_data;
    
    // Start at position 0 in the POST data string
    size_t current_position = 0;
    
    // Keep looping until we've processed the entire string
    while (current_position < post_body.size())
    {
        // Find the next equals sign (separates key from value)
        // Example: in "email=test@test.com", the = is at position 5
        size_t equals_position = post_body.find('=', current_position);
        
        // If we can't find an equals sign, we're done parsing
        if (equals_position == string::npos)
        {
            break;
        }
        
        // Find the next ampersand (separates different key-value pairs)
        // Example: in "email=test&password=secret", the & is between pairs
        size_t ampersand_position = post_body.find('&', equals_position);
        
        // Extract the key (everything from current position to the equals sign)
        string raw_key = post_body.substr(current_position, equals_position - current_position);
        
        // URL-decode the key (convert %20 to spaces, etc.)
        string decoded_key = url_decode(raw_key);
        
        // Calculate where the value starts (right after the equals sign)
        size_t value_start = equals_position + 1;
        
        // Calculate where the value ends (at the ampersand, or end of string if no ampersand)
        size_t value_end = (ampersand_position == string::npos) ? post_body.size() : ampersand_position;
        
        // Extract the value portion
        string raw_value = post_body.substr(value_start, value_end - value_start);
        
        // URL-decode the value
        string decoded_value = url_decode(raw_value);
        
        // Store this key-value pair in our map
        form_data[decoded_key] = decoded_value;
        
        // If there's no ampersand, this was the last pair
        if (ampersand_position == string::npos)
        {
            break;
        }
        
        // Move to the character after the ampersand to start next pair
        current_position = ampersand_position + 1;
    }
    
    // Return the completed map of all form data
    return form_data;
}

/**
 * FUNCTION: generate_random_hex_string
 * PURPOSE: Create a random string of hexadecimal characters for session tokens
 * EXAMPLE: generate_random_hex_string(8) might return "a3f8b2c4"
 * WHY: Session tokens must be unpredictable to prevent attackers from guessing them
 */
string generate_random_hex_string(size_t desired_length)
{
    // This is a string containing all possible hex digits
    // static means it's created once and reused (optimization)
    static const char* hex_digits = "0123456789abcdef";
    
    // Create a random number generator using hardware randomness
    random_device random_device_instance;
    
    // Seed a high-quality random number generator with the hardware random number
    // mt19937_64 is a Mersenne Twister - a very good random number algorithm
    mt19937_64 random_generator(random_device_instance());
    
    // Create an empty string to build our random hex string
    string hex_string;
    
    // Reserve space for all the characters we'll add (optimization)
    hex_string.reserve(desired_length);
    
    // Loop desired_length times to create that many random characters
    for (size_t character_index = 0; character_index < desired_length; character_index++)
    {
        // Generate a random number and take modulo 16 to get 0-15
        int random_digit_index = random_generator() % 16;
        
        // Use the random index to pick a character from our hex_digits string
        // Add that character to our result
        hex_string += hex_digits[random_digit_index];
    }
    
    // Return the completed random hex string
    return hex_string;
}

/**
 * FUNCTION: send_redirect_with_optional_cookie
 * PURPOSE: Send user to a different page and optionally set a session cookie
 * EXAMPLE: Redirect to index.html after successful login with session cookie
 * WHY: After login/register, we need to take user to the main page with their session
 */
void send_redirect_with_optional_cookie(const string& redirect_url, const string& cookie_value = "")
{
    // Send HTTP status 303 (See Other) which tells browser to go to a different page
    cout << "Status: 303 See Other\r\n";
    
    // If a cookie value was provided (not empty string)
    if (!cookie_value.empty())
    {
        // Set the session cookie in the user's browser
        // session= is the cookie name
        // Path=/ means cookie works for entire website
        // HttpOnly means JavaScript can't access it (security)
        // SameSite=Lax prevents cookie from being sent to other websites (security)
        cout << "Set-Cookie: session=" << cookie_value 
             << "; Path=/; HttpOnly; SameSite=Lax\r\n";
    }
    
    // Tell the browser where to redirect to
    cout << "Location: " << redirect_url << "\r\n";
    
    // Specify we're sending HTML content
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Send a fallback HTML page in case browser doesn't auto-redirect
    // This is what user sees if redirect fails
    cout << "<html><body>Redirecting… <a href='" << redirect_url 
         << "'>continue</a></body></html>";
}

/**
 * FUNCTION: send_html_header
 * PURPOSE: Send the standard HTTP header that tells browser we're sending HTML
 * WHY: Every web response needs headers before the actual content
 */
void send_html_header()
{
    // Content-Type tells browser what kind of data we're sending
    // text/html means we're sending a web page
    // \r\n\r\n is two line breaks - separates headers from content
    cout << "Content-Type: text/html\r\n\r\n";
}

/**
 * FUNCTION: get_session_token_from_cookie
 * PURPOSE: Extract the session token from the HTTP cookie header
 * EXAMPLE: If cookie is "session=abc123xyz", this returns "abc123xyz"
 * WHY: Cookies come as one big string, we need to find and extract our session token
 */
string get_session_token_from_cookie()
{
    // Try to get the HTTP_COOKIE environment variable
    // This contains all cookies the browser sent
    const char* cookie_header = getenv("HTTP_COOKIE");
    
    // If there are no cookies, return empty string
    if (!cookie_header) return "";
    
    // Convert the C-style string to a C++ string for easier manipulation
    string cookies(cookie_header);
    
    // Look for "session=" in the cookie string
    size_t session_pos = cookies.find("session=");
    
    // If "session=" wasn't found, return empty string
    if (session_pos == string::npos) return "";
    
    // Calculate where the session value starts (after "session=")
    // "session=" is 8 characters long, so add 8
    size_t start_pos = session_pos + 8;
    
    // Find the semicolon that ends this cookie (or end of string if it's the last cookie)
    size_t end_pos = cookies.find(';', start_pos);
    
    // If no semicolon found, this cookie goes to the end of the string
    if (end_pos == string::npos) end_pos = cookies.length();
    
    // Extract and return just the session token value
    return cookies.substr(start_pos, end_pos - start_pos);
}

/**
 * FUNCTION: get_user_from_session
 * PURPOSE: Check if a session is valid and return the user ID
 * RETURNS: true if session is valid, false if expired or invalid
 * WHY: Every protected page needs to verify the user is logged in
 */
bool get_user_from_session(DatabaseConnection& database, unsigned long long& out_user_id)
{
    // Get the session token from the cookie
    string session_token = get_session_token_from_cookie();
    
    // If there's no session token, user isn't logged in
    if (session_token.empty()) return false;
    
    // SQL query to find a session that:
    // 1. Matches the token
    // 2. Was active in the last 5 minutes
    // DATE_SUB(NOW(), INTERVAL 5 MINUTE) = current time minus 5 minutes
    const char* session_check_sql = 
        "SELECT user_id FROM sessions "
        "WHERE id = ? AND last_active > DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    
    // Create a prepared statement (prevents SQL injection attacks)
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare the SQL statement (compile it)
    // If preparation fails, clean up and return false
    if (mysql_stmt_prepare(session_statement, session_check_sql, strlen(session_check_sql)))
    {
        // Close the statement to free memory
        mysql_stmt_close(session_statement);
        // Return false to indicate session validation failed
        return false;
    }
    
    // Create a binding structure for the parameter (the ?)
    MYSQL_BIND param_binding{};
    
    // Tell MySQL this parameter is a string
    param_binding.buffer_type = MYSQL_TYPE_STRING;
    
    // Point to our session token string
    param_binding.buffer = (void*)session_token.c_str();
    
    // Tell MySQL how long the string is
    param_binding.buffer_length = session_token.size();
    
    // Bind the parameter to the statement (replace ? with our token)
    mysql_stmt_bind_param(session_statement, &param_binding);
    
    // Execute the query
    mysql_stmt_execute(session_statement);
    
    // Create a binding structure for the result (user_id)
    MYSQL_BIND result_binding{};
    
    // Tell MySQL we expect a BIGINT (large integer) back
    result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    
    // Point to our out_user_id variable where MySQL will store the result
    result_binding.buffer = &out_user_id;
    
    // Bind the result structure to receive query results
    mysql_stmt_bind_result(session_statement, &result_binding);
    
    // Try to fetch a row from the results
    // mysql_stmt_fetch returns 0 if a row was found
    // session_valid will be true if we found a matching, non-expired session
    bool session_valid = (mysql_stmt_fetch(session_statement) == 0);
    
    // Close the statement to free resources
    mysql_stmt_close(session_statement);
    
    // If the session is valid, update the last_active timestamp
    if (session_valid)
    {
        // SQL to update the last_active time to right now
        // This keeps active users logged in
        const char* update_sql = "UPDATE sessions SET last_active = NOW() WHERE id = ?";
        
        // Create a new prepared statement for the update
        MYSQL_STMT* update_stmt = mysql_stmt_init(database.mysql_connection);
        
        // Prepare the update statement
        mysql_stmt_prepare(update_stmt, update_sql, strlen(update_sql));
        
        // Bind the session token parameter (reuse param_binding from before)
        mysql_stmt_bind_param(update_stmt, &param_binding);
        
        // Execute the update
        mysql_stmt_execute(update_stmt);
        
        // Close the update statement
        mysql_stmt_close(update_stmt);
    }
    
    // Return whether the session was valid
    return session_valid;
}

/**
 * STRUCT: DatabaseConnection
 * PURPOSE: Manages MySQL database connection using RAII pattern
 * RAII = Resource Acquisition Is Initialization
 * WHY: Automatically opens connection when created, closes when destroyed
 */
struct DatabaseConnection
{
    // Pointer to the MySQL connection object
    // nullptr means "no connection yet"
    MYSQL* mysql_connection = nullptr;
    
    // CONSTRUCTOR: This runs automatically when DatabaseConnection is created
    // It establishes the connection to MySQL
    DatabaseConnection()
    {
        // Initialize a MySQL connection structure
        mysql_connection = mysql_init(nullptr);
        
        // Connect to MySQL server with our credentials
        // Parameters: connection, host, username, password, database, port, socket, flags
        mysql_real_connect(
            mysql_connection,              // The connection we just initialized
            DB_HOST.c_str(),              // Server address (localhost)
            DB_USER.c_str(),              // Username
            DB_PASS.c_str(),              // Password
            DB_NAME.c_str(),              // Which database to use
            0,                             // Port (0 = use default MySQL port 3306)
            nullptr,                       // Unix socket (not used on Windows)
            0                              // Client flags (none)
        );
    }
    
    // DESTRUCTOR: This runs automatically when DatabaseConnection is destroyed
    // It closes the MySQL connection and frees memory
    ~DatabaseConnection()
    {
        // If we have an open connection
        if (mysql_connection != nullptr)
        {
            // Close it to free resources
            mysql_close(mysql_connection);
        }
    }
};

/**
 * FUNCTION: register_new_user
 * PURPOSE: Create a new user account in the database
 * RETURNS: true if successful, false if failed (maybe email already exists)
 * WHY: Users need accounts to participate in auctions
 */
bool register_new_user(DatabaseConnection& database, 
                       const string& user_email, 
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // SQL to insert a new user with hashed password
    // SHA2(CONCAT(?, ?), 224) combines password + pepper and hashes it
    // This creates a 56-character fingerprint that can't be reversed
    const char* insert_user_sql = 
        "INSERT INTO users(email, password_hash) "
        "VALUES(?, SHA2(CONCAT(?, ?), 224))";
    
    // Create a prepared statement to prevent SQL injection
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare (compile) the SQL statement
    // If it fails, close the statement and return false
    if (mysql_stmt_prepare(insert_statement, insert_user_sql, strlen(insert_user_sql)))
    {
        mysql_stmt_close(insert_statement);
        return false;
    }
    
    // Create an array of 3 parameter bindings (one for each ?)
    // {} initializes all values to zero/null
    MYSQL_BIND insert_bindings[3]{};
    
    // FIRST PARAMETER: email address
    insert_bindings[0].buffer_type = MYSQL_TYPE_STRING;              // It's a text string
    insert_bindings[0].buffer = (void*)user_email.c_str();         // Point to the email string
    insert_bindings[0].buffer_length = user_email.size();          // Length of the email
    
    // SECOND PARAMETER: password (will be hashed by MySQL)
    insert_bindings[1].buffer_type = MYSQL_TYPE_STRING;              // It's a text string
    insert_bindings[1].buffer = (void*)user_password.c_str();      // Point to the password string
    insert_bindings[1].buffer_length = user_password.size();       // Length of the password
    
    // THIRD PARAMETER: pepper (secret value added to password)
    insert_bindings[2].buffer_type = MYSQL_TYPE_STRING;              // It's a text string
    insert_bindings[2].buffer = (void*)PEPPER.c_str();             // Point to the pepper constant
    insert_bindings[2].buffer_length = PEPPER.size();              // Length of the pepper
    
    // Bind all parameters to the statement (replace all ? marks)
    mysql_stmt_bind_param(insert_statement, insert_bindings);
    
    // Execute the INSERT statement
    // mysql_stmt_execute returns 0 on success
    bool insert_successful = (mysql_stmt_execute(insert_statement) == 0);
    
    // Close the insert statement to free resources
    mysql_stmt_close(insert_statement);
    
    // If the insert failed (maybe email already exists), return false
    if (!insert_successful)
    {
        return false;
    }
    
    // Now we need to get the ID of the user we just created
    // SQL to find the user by their email address
    const char* select_id_sql = "SELECT id FROM users WHERE email=?";
    
    // Create a new prepared statement for the SELECT query
    MYSQL_STMT* select_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare the SELECT statement
    mysql_stmt_prepare(select_statement, select_id_sql, strlen(select_id_sql));
    
    // Create a binding for the email parameter
    MYSQL_BIND select_param_binding{};
    select_param_binding.buffer_type = MYSQL_TYPE_STRING;
    select_param_binding.buffer = (void*)user_email.c_str();
    select_param_binding.buffer_length = user_email.size();
    
    // Bind the parameter and execute the query
    mysql_stmt_bind_param(select_statement, &select_param_binding);
    mysql_stmt_execute(select_statement);
    
    // Create a binding to receive the user ID result
    MYSQL_BIND select_result_binding{};
    select_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;        // We expect a big integer
    select_result_binding.buffer = &out_user_id;                    // Store it in out_user_id
    
    // Bind the result structure
    mysql_stmt_bind_result(select_statement, &select_result_binding);
    
    // Fetch the result row
    // If successful, out_user_id now contains the new user's ID
    bool fetch_successful = (mysql_stmt_fetch(select_statement) == 0);
    
    // Close the select statement
    mysql_stmt_close(select_statement);
    
    // Return whether we successfully got the user ID
    return fetch_successful;
}

/**
 * FUNCTION: verify_user_login
 * PURPOSE: Check if email and password match a user in the database
 * RETURNS: true if credentials are correct, false if wrong email/password
 * WHY: Users must prove their identity to access their account
 */
bool verify_user_login(DatabaseConnection& database,
                       const string& user_email,
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // SQL query to find user where:
    // 1. Email matches
    // 2. Password hash matches SHA2(password + pepper)
    // If both match, we found the right user
    const char* login_verification_sql =
        "SELECT id FROM users "
        "WHERE email=? AND password_hash = SHA2(CONCAT(?, ?), 224)";
    
    // Create a prepared statement
    MYSQL_STMT* login_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare the statement, return false if it fails
    if (mysql_stmt_prepare(login_statement, login_verification_sql, strlen(login_verification_sql)))
    {
        mysql_stmt_close(login_statement);
        return false;
    }
    
    // Create bindings for 3 parameters (email, password, pepper)
    MYSQL_BIND login_bindings[3]{};
    
    // Bind email parameter
    login_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    login_bindings[0].buffer = (void*)user_email.c_str();
    login_bindings[0].buffer_length = user_email.size();
    
    // Bind password parameter
    login_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    login_bindings[1].buffer = (void*)user_password.c_str();
    login_bindings[1].buffer_length = user_password.size();
    
    // Bind pepper parameter
    login_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    login_bindings[2].buffer = (void*)PEPPER.c_str();
    login_bindings[2].buffer_length = PEPPER.size();
    
    // Bind all parameters and execute the query
    mysql_stmt_bind_param(login_statement, login_bindings);
    mysql_stmt_execute(login_statement);
    
    // Create binding to receive the user ID
    MYSQL_BIND login_result_binding{};
    login_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    login_result_binding.buffer = &out_user_id;
    
    // Bind the result
    mysql_stmt_bind_result(login_statement, &login_result_binding);
    
    // Try to fetch a result row
    // If we get a row, credentials were correct
    bool login_successful = (mysql_stmt_fetch(login_statement) == 0);
    
    // Close the statement
    mysql_stmt_close(login_statement);
    
    // Return whether login was successful
    return login_successful;
}

/**
 * FUNCTION: create_or_update_session
 * PURPOSE: Store or update a session in the database
 * WHY: Sessions track who is logged in and when they were last active
 */
void create_or_update_session(DatabaseConnection& database,
                              unsigned long long user_id,
                              const string& session_token)
{
    // SQL to insert or replace a session
    // REPLACE means: if session ID exists, update it; if not, insert it
    // NOW() gets the current timestamp from MySQL
    const char* session_upsert_sql = 
        "REPLACE INTO sessions(id, user_id, last_active) VALUES(?, ?, NOW())";
    
    // Create a prepared statement
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    
    // Prepare the statement
    mysql_stmt_prepare(session_statement, session_upsert_sql, strlen(session_upsert_sql));
    
    // Create bindings for 2 parameters (session token and user ID)
    MYSQL_BIND session_bindings[2]{};
    
    // Bind session token parameter
    session_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    session_bindings[0].buffer = (void*)session_token.c_str();
    session_bindings[0].buffer_length = session_token.size();
    
    // Bind user ID parameter
    session_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;
    session_bindings[1].buffer = &user_id;
    
    // Bind parameters and execute
    mysql_stmt_bind_param(session_statement, session_bindings);
    mysql_stmt_execute(session_statement);
    
    // Close the statement
    mysql_stmt_close(session_statement);
}

/**
 * FUNCTION: main
 * PURPOSE: Main entry point for the authentication CGI program
 * HANDLES: User registration, login, and logout
 * WHY: This is what runs when someone submits an auth form
 */
int main()
{
    // Turn off synchronization between C and C++ I/O for faster output
    // This is a performance optimization
    ios::sync_with_stdio(false);
    
    // Get the HTTP request method from environment variable
    // REQUEST_METHOD tells us if this is GET, POST, etc.
    const char* request_method_env = getenv("REQUEST_METHOD");
    
    // If REQUEST_METHOD exists, use it; otherwise default to "GET"
    string request_method = request_method_env ? request_method_env : "GET";
    
    // Only allow POST requests for security
    // GET requests put data in URL which is visible in browser history
    // POST requests hide data in the request body
    if (request_method != "POST")
    {
        // Send HTML header
        cout << "Content-Type: text/html\r\n\r\n"
             // Tell user to use the auth.html forms
             << "<p>Use the forms on <a href='auth.html'>auth.html</a>.</p>";
        // Exit program with success code
        return 0;
    }
    
    // Parse the POST data into a map of key-value pairs
    // Example: {"action": "login", "email": "user@test.com", "password": "secret"}
    auto form_data = parse_post_data();
    
    // Extract the three main form fields
    string requested_action = form_data["action"];      // What action to perform (register/login/logout)
    string user_email = form_data["email"];             // User's email address
    string user_password = form_data["password"];       // User's password
    
    // Create a database connection (automatically connects in constructor)
    DatabaseConnection database;
    
    // HANDLE REGISTRATION
    if (requested_action == "register")
    {
        // Validate minimum input requirements
        // Email must be at least 3 characters
        // Password must be at least 8 characters
        if (user_email.size() < 3 || user_password.size() < 8)
        {
            // Send error message back to user
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Invalid input. <a href='auth.html'>Back</a></p>";
            return 0;
        }
        
        // Variable to store the new user's ID
        unsigned long long new_user_id = 0;
        
        // Try to register the user
        // This will fail if email already exists (database constraint)
        if (!register_new_user(database, user_email, user_password, new_user_id))
        {
            // Registration failed - probably duplicate email
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Registration failed (email may exist). <a href='auth.html'>Back</a></p>";
            return 0;
        }
        
        // Registration successful! Create a session
        // Generate a random 64-character session token
        string session_token = generate_random_hex_string(64);
        
        // Store the session in database
        create_or_update_session(database, new_user_id, session_token);
        
        // Redirect user to home page and set their session cookie
        send_redirect_with_optional_cookie("index.html", session_token);
        
        // Exit program successfully
        return 0;
    }
    
    // HANDLE LOGIN
    if (requested_action == "login")
    {
        // Variable to store the user's ID if login succeeds
        unsigned long long existing_user_id = 0;
        
        // Try to verify the login credentials
        // This checks if email exists and password hash matches
        if (!verify_user_login(database, user_email, user_password, existing_user_id))
        {
            // Login failed - wrong email or password
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Login failed. <a href='auth.html'>Try again</a></p>";
            return 0;
        }
        
        // Login successful! Create a session
        // Generate a random 64-character session token
        string session_token = generate_random_hex_string(64);
        
        // Store the session in database
        create_or_update_session(database, existing_user_id, session_token);
        
        // Redirect user to home page and set their session cookie
        send_redirect_with_optional_cookie("index.html", session_token);
        
        // Exit program successfully
        return 0;
    }
    
    // HANDLE LOGOUT
    if (requested_action == "logout")
    {
        // Send redirect with cookie that expires immediately
        // Status: 303 See Other = redirect to another page
        cout << "Status: 303 See Other\r\n"
             // Set cookie with Max-Age=0 which deletes it immediately
             << "Set-Cookie: session=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax\r\n"
             // Redirect to home page
             << "Location: index.html\r\n"
             // Specify HTML content type
             << "Content-Type: text/html\r\n\r\n"
             // Show logout message
             << "Logged out.";
        
        // Exit program successfully
        return 0;
    }
    
    // If we get here, action wasn't register, login, or logout
    // This means unsupported action - show error
    cout << "Content-Type: text/html\r\n\r\n"
         << "<p>Unsupported action. <a href='auth.html'>Back</a></p>";
    
    // Exit program successfully
    return 0;
}