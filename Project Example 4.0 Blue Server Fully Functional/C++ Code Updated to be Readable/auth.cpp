// Includes iostream/strings/containers/algorithms etc. via a single GCC header for brevity
#include <bits/stdc++.h>
// MariaDB/MySQL C API header
#include <mysql/mysql.h>

// Bring std symbols into the global namespace for concise code
using namespace std;

// Database connection constants
const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// Site-wide PEPPER added to passwords before hashing (kept constant here)
const string PEPPER = "site_pepper_2025_minimal_demo";

/**
 * URL-decodes application/x-www-form-urlencoded strings
 * Handles + symbols as spaces and %HH hex encodings
 */
string url_decode(const string &encoded_string)
{
    string output_string;
    output_string.reserve(encoded_string.size());
    
    for (size_t current_index = 0; current_index < encoded_string.size(); ++current_index)
    {
        if (encoded_string[current_index] == '+')
        {
            // Convert '+' to space
            output_string += ' ';
        }
        else if (encoded_string[current_index] == '%' && 
                 current_index + 2 < encoded_string.size())
        {
            // Handle hex encoding %HH
            string hex_string = encoded_string.substr(current_index + 1, 2);
            char decoded_char = static_cast<char>(strtol(hex_string.c_str(), nullptr, 16));
            output_string += decoded_char;
            current_index += 2; // Skip the next two characters
        }
        else
        {
            // Regular character, copy as-is
            output_string += encoded_string[current_index];
        }
    }
    
    return output_string;
}

/**
 * Reads POST body from stdin using CONTENT_LENGTH environment variable
 * Parses form data and returns a key/value map
 */
map<string, string> parse_post_data()
{
    // Get content length from environment variable
    const char* content_length_env = getenv("CONTENT_LENGTH");
    string content_length_string = content_length_env ? content_length_env : "0";
    int content_length = atoi(content_length_string.c_str());
    
    // Read the POST body
    string post_body(max(0, content_length), '\0');
    if (content_length > 0)
    {
        fread(&post_body[0], 1, content_length, stdin);
    }
    
    // Parse the form data into key-value pairs
    map<string, string> form_data;
    size_t current_position = 0;
    
    while (current_position < post_body.size())
    {
        // Find the equals sign separating key from value
        size_t equals_position = post_body.find('=', current_position);
        if (equals_position == string::npos)
        {
            break; // No more key-value pairs
        }
        
        // Find the ampersand separating this pair from the next
        size_t ampersand_position = post_body.find('&', equals_position);
        
        // Extract and decode the key
        string raw_key = post_body.substr(current_position, equals_position - current_position);
        string decoded_key = url_decode(raw_key);
        
        // Extract and decode the value
        size_t value_start = equals_position + 1;
        size_t value_end = (ampersand_position == string::npos) ? post_body.size() : ampersand_position;
        string raw_value = post_body.substr(value_start, value_end - value_start);
        string decoded_value = url_decode(raw_value);
        
        // Store the key-value pair
        form_data[decoded_key] = decoded_value;
        
        // Move to next pair or exit if this was the last one
        if (ampersand_position == string::npos)
        {
            break;
        }
        current_position = ampersand_position + 1;
    }
    
    return form_data;
}

/**
 * Produces a random hexadecimal string of specified length
 * Used for generating session tokens
 */
string generate_random_hex_string(size_t desired_length)
{
    static const char* hex_digits = "0123456789abcdef";
    
    // Initialize random number generator
    random_device random_device_instance;
    mt19937_64 random_generator(random_device_instance());
    
    // Build the hex string
    string hex_string;
    hex_string.reserve(desired_length);
    
    for (size_t character_index = 0; character_index < desired_length; character_index++)
    {
        int random_digit_index = random_generator() % 16;
        hex_string += hex_digits[random_digit_index];
    }
    
    return hex_string;
}

/**
 * Sends a 303 redirect response and optionally sets a session cookie
 */
void send_redirect_with_optional_cookie(const string& redirect_url, const string& cookie_value = "")
{
    // Send HTTP redirect status
    cout << "Status: 303 See Other\r\n";
    
    // Set session cookie if provided
    if (!cookie_value.empty())
    {
        cout << "Set-Cookie: session=" << cookie_value 
             << "; Path=/; HttpOnly; SameSite=Lax\r\n";
    }
    
    // Send redirect location and content type headers
    cout << "Location: " << redirect_url << "\r\n";
    cout << "Content-Type: text/html\r\n\r\n";
    
    // Send fallback HTML for browsers that don't follow redirects automatically
    cout << "<html><body>Redirecting… <a href='" << redirect_url 
         << "'>continue</a></body></html>";
}

/**
 * RAII wrapper for MySQL database connection
 * Opens connection in constructor, closes in destructor
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
 * Registers a new user with email and password
 * Password is hashed with SHA2-224 along with site pepper
 * Returns true on success and sets out_user_id to the new user's ID
 */
bool register_new_user(DatabaseConnection& database, 
                       const string& user_email, 
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // SQL query to insert new user with hashed password
    const char* insert_user_sql = 
        "INSERT INTO users(email, password_hash) "
        "VALUES(?, SHA2(CONCAT(?, ?), 224))";
    
    // Prepare the SQL statement
    MYSQL_STMT* insert_statement = mysql_stmt_init(database.mysql_connection);
    if (mysql_stmt_prepare(insert_statement, insert_user_sql, strlen(insert_user_sql)))
    {
        mysql_stmt_close(insert_statement);
        return false;
    }
    
    // Bind parameters for the insert statement
    MYSQL_BIND insert_bindings[3]{};
    
    // Bind email parameter
    insert_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    insert_bindings[0].buffer = (void*)user_email.c_str();
    insert_bindings[0].buffer_length = user_email.size();
    
    // Bind password parameter  
    insert_bindings[1].buffer_type = MYSQL_TYPE_STRING;
    insert_bindings[1].buffer = (void*)user_password.c_str();
    insert_bindings[1].buffer_length = user_password.size();
    
    // Bind pepper parameter
    insert_bindings[2].buffer_type = MYSQL_TYPE_STRING;
    insert_bindings[2].buffer = (void*)PEPPER.c_str();
    insert_bindings[2].buffer_length = PEPPER.size();
    
    // Execute the insert statement
    mysql_stmt_bind_param(insert_statement, insert_bindings);
    bool insert_successful = (mysql_stmt_execute(insert_statement) == 0);
    mysql_stmt_close(insert_statement);
    
    if (!insert_successful)
    {
        return false;
    }
    
    // Now retrieve the ID of the newly created user
    const char* select_id_sql = "SELECT id FROM users WHERE email=?";
    
    MYSQL_STMT* select_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(select_statement, select_id_sql, strlen(select_id_sql));
    
    // Bind email parameter for the select query
    MYSQL_BIND select_param_binding{};
    select_param_binding.buffer_type = MYSQL_TYPE_STRING;
    select_param_binding.buffer = (void*)user_email.c_str();
    select_param_binding.buffer_length = user_email.size();
    
    mysql_stmt_bind_param(select_statement, &select_param_binding);
    mysql_stmt_execute(select_statement);
    
    // Bind result parameter to receive the user ID
    MYSQL_BIND select_result_binding{};
    select_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    select_result_binding.buffer = &out_user_id;
    
    mysql_stmt_bind_result(select_statement, &select_result_binding);
    
    // Fetch the result and check if successful
    bool fetch_successful = (mysql_stmt_fetch(select_statement) == 0);
    mysql_stmt_close(select_statement);
    
    return fetch_successful;
}

/**
 * Verifies user login credentials
 * Compares stored SHA2-224(password+pepper) hash with provided credentials
 * Returns true on success and sets out_user_id to the user's ID
 */
bool verify_user_login(DatabaseConnection& database,
                       const string& user_email,
                       const string& user_password, 
                       unsigned long long& out_user_id)
{
    // SQL query to find user with matching email and password hash
    const char* login_verification_sql =
        "SELECT id FROM users "
        "WHERE email=? AND password_hash = SHA2(CONCAT(?, ?), 224)";
    
    // Prepare the SQL statement
    MYSQL_STMT* login_statement = mysql_stmt_init(database.mysql_connection);
    if (mysql_stmt_prepare(login_statement, login_verification_sql, strlen(login_verification_sql)))
    {
        mysql_stmt_close(login_statement);
        return false;
    }
    
    // Bind parameters for the login verification
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
    
    // Execute the login verification query
    mysql_stmt_bind_param(login_statement, login_bindings);
    mysql_stmt_execute(login_statement);
    
    // Bind result parameter to receive the user ID
    MYSQL_BIND login_result_binding{};
    login_result_binding.buffer_type = MYSQL_TYPE_LONGLONG;
    login_result_binding.buffer = &out_user_id;
    mysql_stmt_bind_result(login_statement, &login_result_binding);
    
    // Fetch the result and check if login was successful
    bool login_successful = (mysql_stmt_fetch(login_statement) == 0);
    mysql_stmt_close(login_statement);
    
    return login_successful;
}

/**
 * Creates or updates a session row in the database
 * Sets last_active timestamp to current time (NOW())
 */
void create_or_update_session(DatabaseConnection& database,
                              unsigned long long user_id,
                              const string& session_token)
{
    // SQL query to insert or replace session data
    const char* session_upsert_sql = 
        "REPLACE INTO sessions(id, user_id, last_active) VALUES(?, ?, NOW())";
    
    // Prepare the SQL statement
    MYSQL_STMT* session_statement = mysql_stmt_init(database.mysql_connection);
    mysql_stmt_prepare(session_statement, session_upsert_sql, strlen(session_upsert_sql));
    
    // Bind parameters for the session upsert
    MYSQL_BIND session_bindings[2]{};
    
    // Bind session token parameter
    session_bindings[0].buffer_type = MYSQL_TYPE_STRING;
    session_bindings[0].buffer = (void*)session_token.c_str();
    session_bindings[0].buffer_length = session_token.size();
    
    // Bind user ID parameter
    session_bindings[1].buffer_type = MYSQL_TYPE_LONGLONG;
    session_bindings[1].buffer = &user_id;
    
    // Execute the session upsert
    mysql_stmt_bind_param(session_statement, session_bindings);
    mysql_stmt_execute(session_statement);
    mysql_stmt_close(session_statement);
}

/**
 * Main CGI entry point
 * Handles user registration, login, and logout operations
 * Enforces POST method for form submissions
 */
int main()
{
    // Optimize I/O performance
    ios::sync_with_stdio(false);
    
    // Get the HTTP request method
    const char* request_method_env = getenv("REQUEST_METHOD");
    string request_method = request_method_env ? request_method_env : "GET";
    
    // Only allow POST requests for security
    if (request_method != "POST")
    {
        cout << "Content-Type: text/html\r\n\r\n"
             << "<p>Use the forms on <a href='auth.html'>auth.html</a>.</p>";
        return 0;
    }
    
    // Parse the POST form data
    auto form_data = parse_post_data();
    string requested_action = form_data["action"];
    string user_email = form_data["email"];
    string user_password = form_data["password"];
    
    // Initialize database connection
    DatabaseConnection database;
    
    // Handle user registration
    if (requested_action == "register")
    {
        // Validate input parameters
        if (user_email.size() < 3 || user_password.size() < 8)
        {
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Invalid input. <a href='auth.html'>Back</a></p>";
            return 0;
        }
        
        // Attempt to register the new user
        unsigned long long new_user_id = 0;
        if (!register_new_user(database, user_email, user_password, new_user_id))
        {
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Registration failed (email may exist). <a href='auth.html'>Back</a></p>";
            return 0;
        }
        
        // Registration successful - create session and redirect
        string session_token = generate_random_hex_string(64);
        create_or_update_session(database, new_user_id, session_token);
        send_redirect_with_optional_cookie("index.html", session_token);
        
        return 0;
    }
    
    // Handle user login
    if (requested_action == "login")
    {
        // Attempt to verify login credentials
        unsigned long long existing_user_id = 0;
        if (!verify_user_login(database, user_email, user_password, existing_user_id))
        {
            cout << "Content-Type: text/html\r\n\r\n"
                 << "<p>Login failed. <a href='auth.html'>Try again</a></p>";
            return 0;
        }
        
        // Login successful - create session and redirect
        string session_token = generate_random_hex_string(64);
        create_or_update_session(database, existing_user_id, session_token);
        send_redirect_with_optional_cookie("index.html", session_token);
        
        return 0;
    }
    
    // Handle user logout
    if (requested_action == "logout")
    {
        // Clear the session cookie and redirect
        cout << "Status: 303 See Other\r\n"
             << "Set-Cookie: session=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax\r\n"
             << "Location: index.html\r\n"
             << "Content-Type: text/html\r\n\r\n"
             << "Logged out.";
        return 0;
    }
    
    // Handle unsupported actions
    cout << "Content-Type: text/html\r\n\r\n"
         << "<p>Unsupported action. <a href='auth.html'>Back</a></p>";
    return 0;
}