// *******
// auth.cpp
// *******


// This line includes a special header that gives us access to common C++ features like cout, string, map, etc.
#include <bits/stdc++.h>
// This line includes the MySQL library so we can connect to and use our database
#include <mysql/mysql.h>

// This line lets us use standard library features without typing "std::" every time
using namespace std;

// These are constants that store our database connection information
// DB_HOST is the server address where our database lives (localhost means this computer)
const string DB_HOST = "localhost";
// DB_USER is the username we use to log into the database
const string DB_USER = "allindragons";
// DB_PASS is the password for that database user
const string DB_PASS = "snogardnilla_002";
// DB_NAME is the specific database we want to use on that server
const string DB_NAME = "cs370_section2_allindragons";

// This is our "pepper" - a secret string we add to passwords before hashing them for extra security
const string PEPPER = "site_pepper_2025_minimal_demo";

// This function converts URL-encoded strings back to normal text
// For example, "hello+world" becomes "hello world" and "%20" becomes a space
string url_decode(const string &encoded_string)
{
    // Create an empty string to store our decoded output
    string output_string;
    // Reserve space in memory to make string building more efficient
    int size_needed = encoded_string.size();
    // Loop through and add placeholder characters
    for(int i = 0; i < size_needed; i++) {
        output_string += 'x'; // Add a temporary character
    }
    // Now clear it out (this is inefficient but shows the process)
    output_string = "";
    
    // Start at the beginning of the string
    int current_index = 0;
    // Keep going until we've processed every character
    while(current_index < encoded_string.size())
    {
        // Get the current character we're looking at
        char c = encoded_string[current_index];
        // Check if it's a plus sign (which means space in URL encoding)
        if (c == '+')
        {
            // Add a space to our output
            output_string += ' ';
            // Move to the next character
            current_index++;
        }
        // Check if it's a percent sign (which starts a hex code like %20)
        else if (c == '%')
        {
            // Make sure there are at least 2 more characters after the %
            if(current_index + 2 < encoded_string.size()) {
                // Create a string to hold the two hex digits
                string hex_string = "";
                // Get the first hex digit
                hex_string += encoded_string[current_index + 1];
                // Get the second hex digit
                hex_string += encoded_string[current_index + 2];
                // Convert the hex string to an actual character (strtol converts string to number)
                char decoded_char = (char)strtol(hex_string.c_str(), nullptr, 16);
                // Add that character to our output
                output_string += decoded_char;
                // Skip ahead 3 positions (past the % and 2 hex digits)
                current_index += 3;
            } else {
                // If there aren't enough characters, just skip the %
                current_index++;
            }
        }
        // If it's a normal character
        else
        {
            // Just add it to our output as-is
            output_string += c;
            // Move to the next character
            current_index++;
        }
    }
    
    // Return the fully decoded string
    return output_string;
}

// This function reads form data that was submitted via POST method
// It returns a map (like a dictionary) with key-value pairs from the form
map<string, string> parse_post_data()
{
    // Get the CONTENT_LENGTH environment variable which tells us how much data is coming
    const char* content_length_env = getenv("CONTENT_LENGTH");
    // Create a string to store the content length value
    string content_length_string;
    // Check if the environment variable exists
    if(content_length_env) {
        // If it exists, store its value
        content_length_string = content_length_env;
    } else {
        // If it doesn't exist, default to "0" (no data)
        content_length_string = "0";
    }
    // Convert the string to an integer so we know how many bytes to read
    int content_length = atoi(content_length_string.c_str());
    
    // Create a string to hold all the POST data
    string post_body = "";
    // Only read data if there's actually some coming in
    if(content_length > 0) {
        // Make the string big enough to hold all the data by adding null characters
        for(int i = 0; i < content_length; i++) {
            post_body += '\0'; // Add a null character as a placeholder
        }
        // Actually read the data from standard input (where POST data comes from)
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    // Create a map to store the parsed key-value pairs
    map<string, string> form_data;
    // Start at the beginning of the POST data
    int current_position = 0;
    
    // Keep looping until we've parsed all the data
    while (true)
    {
        // Check if we've reached the end of the data
        if(current_position >= post_body.size()) {
            break; // Exit the loop - we're done
        }
        
        // Find the next equals sign (separates keys from values)
        int equals_position = -1; // -1 means "not found yet"
        // Search through the string starting from current position
        for(int i = current_position; i < post_body.size(); i++) {
            // Check if this character is an equals sign
            if(post_body[i] == '=') {
                equals_position = i; // Remember where we found it
                break; // Stop searching
            }
        }
        
        // If we didn't find an equals sign, we're done parsing
        if (equals_position == -1)
        {
            break; // Exit the loop
        }
        
        // Find the next ampersand (separates different key-value pairs)
        int ampersand_position = -1; // -1 means "not found yet"
        // Search starting from after the equals sign
        for(int i = equals_position; i < post_body.size(); i++) {
            // Check if this character is an ampersand
            if(post_body[i] == '&') {
                ampersand_position = i; // Remember where we found it
                break; // Stop searching
            }
        }
        
        // Extract the key (everything before the equals sign)
        string raw_key = ""; // Create empty string for the key
        // Copy characters from current position to the equals sign
        for(int i = current_position; i < equals_position; i++) {
            raw_key += post_body[i]; // Add each character
        }
        // Decode the key in case it has URL encoding
        string decoded_key = url_decode(raw_key);
        
        // Extract the value (everything after the equals sign)
        string raw_value = ""; // Create empty string for the value
        int value_start = equals_position + 1; // Start right after the equals
        int value_end; // Where the value ends
        // Check if we found an ampersand
        if(ampersand_position == -1) {
            value_end = post_body.size(); // If no ampersand, value goes to end of string
        } else {
            value_end = ampersand_position; // Otherwise, value ends at the ampersand
        }
        // Copy characters from value start to value end
        for(int i = value_start; i < value_end; i++) {
            raw_value += post_body[i]; // Add each character
        }
        // Decode the value in case it has URL encoding
        string decoded_value = url_decode(raw_value);
        
        // Store the key-value pair in our map
        form_data[decoded_key] = decoded_value;
        
        // Check if this was the last key-value pair
        if (ampersand_position == -1)
        {
            break; // Exit the loop - we're done
        }
        // Move to the next key-value pair (start after the ampersand)
        current_position = ampersand_position + 1;
    }
    
    // Return the map containing all the form data
    return form_data;
}

// This function generates a random hexadecimal string for session tokens
// Hex digits are 0-9 and a-f, so "3a5f9b" would be a valid hex string
string generate_random_hex_string(size_t desired_length)
{
    // This string contains all possible hex digits
    string hex_digits = "0123456789abcdef";
    
    // Create a random device to get truly random numbers
    random_device random_device_instance;
    // Create a random number generator using the random device as a seed
    mt19937_64 random_generator(random_device_instance());
    
    // Create an empty string to build our hex string
    string hex_string = "";
    
    // Start a counter at 0
    int character_index = 0;
    // Keep looping until we've generated enough characters
    while(character_index < desired_length)
    {
        // Generate a random number between 0 and 15
        int random_digit_index = random_generator() % 16;
        // Get the hex digit at that position
        char digit = hex_digits[random_digit_index];
        // Add it to our hex string
        hex_string += digit;
        // Increment the counter
        character_index++;
    }
    
    // Return the completed random hex string
    return hex_string;
}

// This function sends an HTTP redirect response to the browser
// It can optionally set a cookie at the same time
void send_redirect_with_optional_cookie(const string& redirect_url, const string& cookie_value = "")
{
    // Send the HTTP status code for redirect
    cout << "Status: 303 See Other\r\n";
    
    // Check if we were given a cookie value to set
    if (cookie_value != "")
    {
        // Send the Set-Cookie header with our session token
        cout << "Set-Cookie: session=" << cookie_value 
             << "; Path=/; HttpOnly; SameSite=Lax\r\n"; // Cookie settings for security
    }
    
    // Send the Location header telling the browser where to go
    cout << "Location: " << redirect_url << "\r\n";
    // Send the content type header
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Send backup HTML in case the browser doesn't auto-redirect
    cout << "<html><body>Redirecting… <a href='" << redirect_url 
         << "'>continue</a></body></html>";
}

// This is a struct (like a class) that manages our database connection
// It automatically connects when created and disconnects when destroyed
struct DatabaseConnection
{
    // This pointer will hold our MySQL connection
    MYSQL* mysql_connection;
    
    // Constructor - this runs when we create a DatabaseConnection object
    DatabaseConnection()
    {
        // Initialize the pointer to null (nothing) first
        mysql_connection = nullptr;
        // Initialize the MySQL library
        mysql_connection = mysql_init(nullptr);
        
        // Actually connect to the database using our credentials
        MYSQL* result = mysql_real_connect(
            mysql_connection,      // The connection object we initialized
            DB_HOST.c_str(),       // Convert our host string to C-style string
            DB_USER.c_str(),       // Convert our username to C-style string
            DB_PASS.c_str(),       // Convert our password to C-style string
            DB_NAME.c_str(),       // Convert our database name to C-style string
            0,                     // Port number (0 means use default)
            nullptr,               // Unix socket (nullptr means don't use one)
            0                      // Client flags (0 means no special flags)
        );
        
        // Check if connection succeeded (but we don't actually handle errors here)
        if(result == nullptr) {
            // Connection failed but we're not doing anything about it
        }
    }
    
    // Destructor - this runs automatically when the object is destroyed
    ~DatabaseConnection()
    {
        // Check if we have a valid connection
        if (mysql_connection != nullptr)
        {
            // Close the database connection to free up resources
            mysql_close(mysql_connection);
        }
    }
};

// This function registers a new user in the database
// It returns true if successful, false if it fails
bool register_new_user(DatabaseConnection& database, 
                       const string& user_email, 
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // This is our SQL query that will insert a new user
    // The ? marks are placeholders that we'll fill in with actual values
    // SHA2(CONCAT(?, ?), 224) hashes the password+pepper for security
    const char* insert_user_sql = 
        "INSERT INTO users(email, password_hash) "
        "VALUES(?, SHA2(CONCAT(?, ?), 224))";
    
    // Initialize a prepared statement (this makes queries safer from SQL injection)
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL statement by parsing it
    int prep_result = mysql_stmt_prepare(insert_statement, insert_user_sql, strlen(insert_user_sql));
    // Check if preparing failed
    if (prep_result != 0)
    {
        // Close the statement to free memory
        mysql_stmt_close(insert_statement);
        // Return false to indicate failure
        return false;
    }
    
    // Create an array to hold parameter bindings (how we fill in the ? marks)
    MYSQL_BIND insert_bindings[3]; // We need 3 bindings for our 3 ? marks
    // Zero out all the binding structures first (good practice)
    for(int i = 0; i < 3; i++) {
        insert_bindings[i].buffer_type = MYSQL_TYPE_NULL;  // Set type to null initially
        insert_bindings[i].buffer = nullptr;                // No data buffer yet
        insert_bindings[i].buffer_length = 0;               // No length yet
        insert_bindings[i].is_null = nullptr;               // Not null indicator
        insert_bindings[i].length = nullptr;                // No length variable
        insert_bindings[i].error = nullptr;                 // No error variable
    }
    
    // Set up the first binding for the email parameter
    insert_bindings[0].buffer_type = MYSQL_TYPE_STRING;           // It's a string type
    insert_bindings[0].buffer = (void*)user_email.c_str();        // Point to the email string
    insert_bindings[0].buffer_length = user_email.size();         // Length of the email
    
    // Set up the second binding for the password parameter
    insert_bindings[1].buffer_type = MYSQL_TYPE_STRING;           // It's a string type
    insert_bindings[1].buffer = (void*)user_password.c_str();     // Point to the password string
    insert_bindings[1].buffer_length = user_password.size();      // Length of the password
    
    // Set up the third binding for the pepper parameter
    insert_bindings[2].buffer_type = MYSQL_TYPE_STRING;           // It's a string type
    insert_bindings[2].buffer = (void*)PEPPER.c_str();            // Point to the pepper string
    insert_bindings[2].buffer_length = PEPPER.size();             // Length of the pepper
    
    // Bind the parameters to the statement (connect the bindings to the ? marks)
    mysql_stmt_bind_param(insert_statement, insert_bindings);
    // Execute the statement (actually run the INSERT query)
    int exec_result = mysql_stmt_execute(insert_statement);
    // Check if execution was successful (0 means success)
    bool insert_successful = (exec_result == 0);
    // Close the statement to free memory
    mysql_stmt_close(insert_statement);
    
    // If the insert failed, return false
    if (!insert_successful)
    {
        return false;
    }
    
    // Now we need to get the user ID that was just created
    // This SQL query will find the user we just inserted by their email
    const char* select_id_sql = "SELECT id FROM users WHERE email=?";
    
    // Initialize a new prepared statement for the SELECT query
    MYSQL_STMT* select_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SELECT statement
    mysql_stmt_prepare(select_statement, select_id_sql, strlen(select_id_sql));
    
    // Create a binding for the email parameter in the SELECT query
    MYSQL_BIND select_param_binding;
    select_param_binding.buffer_type = MYSQL_TYPE_STRING;          // It's a string
    select_param_binding.buffer = (void*)user_email.c_str();       // Point to email
    select_param_binding.buffer_length = user_email.size();        // Email length
    select_param_binding.is_null = nullptr;                        // Not null
    select_param_binding.length = nullptr;                         // No length var
    select_param_binding.error = nullptr;                          // No error var
    
    // Bind the email parameter to the SELECT statement
    mysql_stmt_bind_param(select_statement, &select_param_binding);
    // Execute the SELECT query
    mysql_stmt_execute(select_statement);
    
    // Create a binding for the result (the user ID we're getting back)
    MYSQL_BIND select_result_binding;
    select_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;       // It's a big integer
    select_result_binding.buffer = &out_user_id;                   // Store it in out_user_id
    select_result_binding.is_null = nullptr;                       // Not null
    select_result_binding.length = nullptr;                        // No length var
    select_result_binding.error = nullptr;                         // No error var
    
    // Bind the result variable so it receives the user ID
    mysql_stmt_bind_result(select_statement, &select_result_binding);
    
    // Fetch the result row (get the user ID)
    int fetch_result = mysql_stmt_fetch(select_statement);
    // Check if fetch was successful (0 means success)
    bool fetch_successful = (fetch_result == 0);
    // Close the statement to free memory
    mysql_stmt_close(select_statement);
    
    // Return whether we successfully got the user ID
    return fetch_successful;
}

// This function checks if a login attempt is valid
// It returns true if the email and password match, false otherwise
bool verify_user_login(DatabaseConnection& database,
                       const string& user_email,
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // This SQL query finds a user with matching email AND password hash
    // If both match, we know the login is correct
    const char* login_verification_sql =
        "SELECT id FROM users "
        "WHERE email=? AND password_hash = SHA2(CONCAT(?, ?), 224)";
    
    // Initialize a prepared statement for the login check
    MYSQL_STMT* login_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL statement
    int prep_result = mysql_stmt_prepare(login_statement, login_verification_sql, strlen(login_verification_sql));
    // Check if preparation failed
    if (prep_result != 0)
    {
        // Close the statement to free memory
        mysql_stmt_close(login_statement);
        // Return false to indicate failure
        return false;
    }
    
    // Create an array for the three parameter bindings (email, password, pepper)
    MYSQL_BIND login_bindings[3];
    // Zero out all the bindings first
    for(int i = 0; i < 3; i++) {
        login_bindings[i].buffer_type = MYSQL_TYPE_NULL;    // Set type to null initially
        login_bindings[i].buffer = nullptr;                 // No buffer yet
        login_bindings[i].buffer_length = 0;                // No length yet
        login_bindings[i].is_null = nullptr;                // Not null
        login_bindings[i].length = nullptr;                 // No length var
        login_bindings[i].error = nullptr;                  // No error var
    }
    
    // Set up the first binding for email
    login_bindings[0].buffer_type = MYSQL_TYPE_STRING;              // String type
    login_bindings[0].buffer = (void*)user_email.c_str();           // Point to email
    login_bindings[0].buffer_length = user_email.size();            // Email length
    
    // Set up the second binding for password
    login_bindings[1].buffer_type = MYSQL_TYPE_STRING;              // String type
    login_bindings[1].buffer = (void*)user_password.c_str();        // Point to password
    login_bindings[1].buffer_length = user_password.size();         // Password length
    
    // Set up the third binding for pepper
    login_bindings[2].buffer_type = MYSQL_TYPE_STRING;              // String type
    login_bindings[2].buffer = (void*)PEPPER.c_str();               // Point to pepper
    login_bindings[2].buffer_length = PEPPER.size();                // Pepper length
    
    // Bind the parameters to the statement
    mysql_stmt_bind_param(login_statement, login_bindings);
    // Execute the query to check if credentials match
    mysql_stmt_execute(login_statement);
    
    // Create a binding for the result (user ID if login succeeds)
    MYSQL_BIND login_result_binding;
    login_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;         // Big integer
    login_result_binding.buffer = &out_user_id;                     // Store in out_user_id
    login_result_binding.is_null = nullptr;                         // Not null
    login_result_binding.length = nullptr;                          // No length var
    login_result_binding.error = nullptr;                           // No error var
    // Bind the result to receive the user ID
    mysql_stmt_bind_result(login_statement, &login_result_binding);
    
    // Fetch the result (get user ID if found)
    int fetch_result = mysql_stmt_fetch(login_statement);
    // Check if we got a result (0 means we found a matching user)
    bool login_successful = (fetch_result == 0);
    // Close the statement to free memory
    mysql_stmt_close(login_statement);
    
    // Return whether login was successful
    return login_successful;
}

// This function creates a new session or updates an existing one
// Sessions track which user is logged in
void create_or_update_session(DatabaseConnection& database,
                              unsigned long long user_id,
                              const string& session_token)
{
    // This SQL uses REPLACE which either inserts a new row or updates an existing one
    // NOW() sets the last_active time to the current time
    const char* session_upsert_sql = 
        "REPLACE INTO sessions(id, user_id, last_active) VALUES(?, ?, NOW())";
    
    // Initialize a prepared statement
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    // Prepare the SQL statement
    mysql_stmt_prepare(session_statement, session_upsert_sql, strlen(session_upsert_sql));
    
    // Create an array for two parameter bindings (session token and user ID)
    MYSQL_BIND session_bindings[2];
    // Initialize the bindings
    for(int i = 0; i < 2; i++) {
        session_bindings[i].buffer_type = MYSQL_TYPE_NULL;   // Set type to null initially
        session_bindings[i].buffer = nullptr;                // No buffer yet
        session_bindings[i].buffer_length = 0;               // No length yet
        session_bindings[i].is_null = nullptr;               // Not null
        session_bindings[i].length = nullptr;                // No length var
        session_bindings[i].error = nullptr;                 // No error var
    }
    
    // Set up the first binding for session token
    session_bindings[0].buffer_type = MYSQL_TYPE_STRING;             // String type
    session_bindings[0].buffer = (void*)session_token.c_str();       // Point to token
    session_bindings[0].buffer_length = session_token.size();        // Token length
    
    // Create a copy of user_id (we need a non-const variable to point to)
    unsigned long long user_id_copy = user_id;
    // Set up the second binding for user ID
    session_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;           // Big integer
    session_bindings[1].buffer = &user_id_copy;                      // Point to user ID copy
    
    // Bind the parameters to the statement
    mysql_stmt_bind_param(session_statement, session_bindings);
    // Execute the statement to create/update the session
    mysql_stmt_execute(session_statement);
    // Close the statement to free memory
    mysql_stmt_close(session_statement);
}

// This is the main function - where the program starts running
int main()
{
    // Turn off synchronization with C I/O for faster output
    ios::sync_with_stdio(false);
    
    // Get the HTTP request method from the environment (GET, POST, etc.)
    const char* request_method_env = getenv("REQUEST_METHOD");
    // Create a string to store the method
    string request_method;
    // Check if the environment variable exists
    if(request_method_env) {
        // If it exists, store its value
        request_method = request_method_env;
    } else {
        // If it doesn't exist, default to GET
        request_method = "GET";
    }
    
    // Check if this is NOT a POST request
    if (request_method != "POST")
    {
        // Send the content type header
        cout << "Content-Type: text/html\r\n\r\n";
        // Send an error message telling user to use the forms
        cout << "<p>Use the forms on <a href='auth.html'>auth.html</a>.</p>";
        // Exit the program with success code
        return 0;
    }
    
    // Parse the POST data to get the form fields
    auto form_data = parse_post_data();
    // Get the action field (register, login, or logout)
    string requested_action = form_data["action"];
    // Get the email field
    string user_email = form_data["email"];
    // Get the password field
    string user_password = form_data["password"];
    
    // Create a database connection (constructor connects automatically)
    DatabaseConnection database;
    
    // Check if the user wants to register
    if (requested_action == "register")
    {
        // Validate that email is at least 3 characters
        bool email_ok = (user_email.size() >= 3);
        // Validate that password is at least 8 characters
        bool password_ok = (user_password.size() >= 8);
        
        // Check if either validation failed
        if (!email_ok || !password_ok)
        {
            // Send content type header
            cout << "Content-Type: text/html\r\n\r\n";
            // Send error message
            cout << "<p>Invalid input. <a href='auth.html'>Back</a></p>";
            // Exit the program
            return 0;
        }
        
        // Create a variable to store the new user's ID
        unsigned long long new_user_id = 0;
        // Try to register the user
        bool reg_worked = register_new_user(database, user_email, user_password, new_user_id);
        // Check if registration failed
        if (!reg_worked)
        {
            // Send content type header
            cout << "Content-Type: text/html\r\n\r\n";
            // Send error message (probably email already exists)
            cout << "<p>Registration failed (email may exist). <a href='auth.html'>Back</a></p>";
            // Exit the program
            return 0;
        }
        
        // Registration worked! Generate a random session token
        string session_token = generate_random_hex_string(64);
        // Create a session for this user
        create_or_update_session(database, new_user_id, session_token);
        // Redirect to home page and set the session cookie
        send_redirect_with_optional_cookie("index.html", session_token);
        
        // Exit the program
        return 0;
    }
    
    // Check if the user wants to login
    if (requested_action == "login")
    {
        // Create a variable to store the user ID
        unsigned long long existing_user_id = 0;
        // Try to verify the login credentials
        bool login_worked = verify_user_login(database, user_email, user_password, existing_user_id);
        // Check if login failed
        if (!login_worked)
        {
            // Send content type header
            cout << "Content-Type: text/html\r\n\r\n";
            // Send error message
            cout << "<p>Login failed. <a href='auth.html'>Try again</a></p>";
            // Exit the program
            return 0;
        }
        
        // Login worked! Generate a random session token
        string session_token = generate_random_hex_string(64);
        // Create a session for this user
        create_or_update_session(database, existing_user_id, session_token);
        // Redirect to home page and set the session cookie
        send_redirect_with_optional_cookie("index.html", session_token);
        
        // Exit the program
        return 0;
    }
    
    // Check if the user wants to logout
    if (requested_action == "logout")
    {
        // Send redirect status
        cout << "Status: 303 See Other\r\n";
        // Clear the session cookie by setting Max-Age to 0 (expires immediately)
        cout << "Set-Cookie: session=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax\r\n";
        // Redirect to home page
        cout << "Location: index.html\r\n";
        // Send content type header
        cout << "Content-Type: text/html\r\n\r\n";
        // Send logout message
        cout << "Logged out.";
        // Exit the program
        return 0;
    }
    
    // If we get here, the action wasn't recognized
    // Send content type header
    cout << "Content-Type: text/html\r\n\r\n";
    // Send error message
    cout << "<p>Unsupported action. <a href='auth.html'>Back</a></p>";
    // Exit the program
    return 0;
}