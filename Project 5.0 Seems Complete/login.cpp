// login.cpp - Step 2: Authentication System
// This file handles user login, registration, and session management

#include <iostream>      // for input/output operations
#include <string>        // for string handling
#include <ctime>         // for time operations
#include <sstream>       // for string stream operations
#include <mysql/mysql.h> // for MySQL database connection
#include <cstdlib>       // for getenv() to read POST data

using namespace std;

// Database connection information
const string DB_HOST = "localhost";                           // database server location
const string DB_USER = "allindragons";                        // database username
const string DB_PASS = "snogardnilla_002";                    // database password
const string DB_NAME = "cs370_section2_allindragons";         // database name

// Function to decode URL-encoded strings (convert %20 to space, etc.)
string urlDecode(const string& str) {
    string result = "";                  // this will hold our decoded string
    for (size_t i = 0; i < str.length(); i++) {  // loop through each character
        if (str[i] == '+') {             // plus signs become spaces
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {  // % followed by two hex digits
            // Convert hex to character (example: %20 becomes space)
            int hexValue;                // will hold the numeric value
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hexValue);  // read hex value
            result += static_cast<char>(hexValue);  // convert to character
            i += 2;                      // skip the two hex digits we just processed
        } else {
            result += str[i];            // normal character, just copy it
        }
    }
    return result;                       // return the decoded string
}

// Function to parse POST data and extract field values
string getPostValue(const string& postData, const string& fieldName) {
    size_t pos = postData.find(fieldName + "=");  // find where field appears
    if (pos == string::npos) {           // if field not found
        return "";                       // return empty string
    }
    pos += fieldName.length() + 1;       // move past "fieldname="
    size_t endPos = postData.find("&", pos);  // find next field (marked by &)
    string value;                        // will hold the field value
    if (endPos == string::npos) {        // if this is the last field
        value = postData.substr(pos);    // take everything to the end
    } else {
        value = postData.substr(pos, endPos - pos);  // take until next &
    }
    return urlDecode(value);             // decode and return the value
}

// Function to escape special characters for SQL (prevent SQL injection)
string escapeSQL(MYSQL* conn, const string& str) {
    char* escaped = new char[str.length() * 2 + 1];  // allocate enough space
    mysql_real_escape_string(conn, escaped, str.c_str(), str.length());  // escape the string
    string result(escaped);              // convert back to C++ string
    delete[] escaped;                    // free the memory we allocated
    return result;                       // return escaped string
}

// Function to read cookies from browser
string getCookie(const string& cookieName) {
    const char* cookies = getenv("HTTP_COOKIE");  // get all cookies from browser
    if (cookies == NULL) {               // if no cookies exist
        return "";                       // return empty string
    }
    string cookieStr(cookies);           // convert to C++ string
    size_t pos = cookieStr.find(cookieName + "=");  // find our cookie
    if (pos == string::npos) {           // if cookie not found
        return "";                       // return empty string
    }
    pos += cookieName.length() + 1;      // move past "cookiename="
    size_t endPos = cookieStr.find(";", pos);  // cookies separated by semicolon
    if (endPos == string::npos) {        // if this is the last cookie
        return cookieStr.substr(pos);    // return rest of string
    }
    return cookieStr.substr(pos, endPos - pos);  // return cookie value
}

// Function to generate session cookie
void setSessionCookie(int userId) {
    time_t now = time(0);                // get current time
    time_t expireTime = now + 300;       // expire in 5 minutes (300 seconds)
    string cookieValue = to_string(userId) + ":" + to_string(expireTime);  // format: userid:expiretime
    // Send cookie to browser - no path specified so it defaults to current directory
    // CRITICAL: This must be sent BEFORE Content-type header
    cout << "Set-Cookie: session=" << cookieValue << "; HttpOnly\r\n";
}

// Function to check if user session is valid
int checkSession() {
    string sessionCookie = getCookie("session");  // read session cookie
    if (sessionCookie.empty()) {         // if no session cookie exists
        return 0;                        // return 0 meaning not logged in
    }
    
    // Parse cookie value (format: userid:expiretime)
    size_t colonPos = sessionCookie.find(":");  // find the colon separator
    if (colonPos == string::npos) {      // if format is wrong
        return 0;                        // return 0 meaning invalid
    }
    
    int userId = atoi(sessionCookie.substr(0, colonPos).c_str());  // extract user ID
    time_t expireTime = atol(sessionCookie.substr(colonPos + 1).c_str());  // extract expire time
    time_t now = time(0);                // get current time
    
    if (now > expireTime) {              // if session has expired
        return 0;                        // return 0 meaning session expired
    }
    
    return userId;                       // return user ID if session is valid
}

int main() {
    // Read POST data from standard input
    const char* contentLength = getenv("CONTENT_LENGTH");  // get size of POST data
    string postData = "";                // will hold the POST data
    if (contentLength != NULL) {         // if there is POST data
        int length = atoi(contentLength);  // convert length to integer
        char* buffer = new char[length + 1];  // allocate buffer for data
        cin.read(buffer, length);        // read the data from standard input
        buffer[length] = '\0';           // add null terminator
        postData = string(buffer);       // convert to C++ string
        delete[] buffer;                 // free the buffer
    }
    
    // Also check for GET parameters (for logout link)
    const char* queryString = getenv("QUERY_STRING");  // get query string
    string getData = "";                 // will hold GET data
    if (queryString != NULL) {           // if there are GET parameters
        getData = string(queryString);   // convert to C++ string
    }
    
    // Extract form fields from POST data or GET data
    string action = getPostValue(postData, "action");          // try POST first
    if (action.empty()) {                // if not in POST data
        action = getPostValue(getData, "action");              // try GET data
    }
    string email = getPostValue(postData, "email");            // user's email
    string password = getPostValue(postData, "password");      // user's password
    
    // Connect to MySQL database
    MYSQL* conn = mysql_init(NULL);      // initialize MySQL connection object
    if (conn == NULL) {                  // if initialization failed
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 document type declaration
        cout << "<html lang=\"en\">\n";  // open HTML tag with English language
        cout << "  <head>\n";            // open head section
        cout << "    <meta charset=\"UTF-8\">\n";  // set character encoding to UTF-8
        cout << "    <title>Database Error</title>\n";  // set page title
        cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";  // link to stylesheet (now CGI)
        cout << "  </head>\n";           // close head section
        cout << "  <body>\n";            // open body section
        cout << "    <div class=\"container\">\n";  // open container div
        cout << "      <h1>Database Error</h1>\n";  // error heading
        cout << "      <p>Failed to initialize database connection.</p>\n";  // error message
        cout << "      <a href=\"index.cgi\">Go Back</a>\n";  // link to go back
        cout << "    </div>\n";          // close container div
        cout << "  </body>\n";           // close body section
        cout << "</html>\n";             // close HTML tag
        return 1;                        // exit with error code
    }
    
    // Actually connect to the database
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Database Error</title>\n";  // page title
        cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";  // stylesheet link (now CGI)
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <div class=\"container\">\n";  // container div
        cout << "      <h1>Database Error</h1>\n";  // heading
        cout << "      <p>Failed to connect to database: " << mysql_error(conn) << "</p>\n";  // error with details
        cout << "      <a href=\"index.cgi\">Go Back</a>\n";  // back link
        cout << "    </div>\n";          // close container
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
        mysql_close(conn);               // close the connection
        return 1;                        // exit with error code
    }
    
    // Handle registration action
    if (action == "register") {
        // Validate input - only check email and password (no password confirmation)
        if (email.empty() || password.empty()) {
            cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
            cout << "<!DOCTYPE html>\n";  // HTML5 doctype
            cout << "<html lang=\"en\">\n";  // HTML with language
            cout << "  <head>\n";        // head section
            cout << "    <meta charset=\"UTF-8\">\n";  // character set
            cout << "    <title>Registration Error</title>\n";  // page title
            cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";  // link stylesheet (now CGI)
            cout << "  </head>\n";       // close head
            cout << "  <body>\n";        // body section
            cout << "    <div class=\"container\">\n";  // container div
            cout << "      <div class=\"message error\">Email and password are required.</div>\n";  // error message
            cout << "      <a href=\"index.cgi\">Go Back</a>\n";  // back link
            cout << "    </div>\n";      // close container
            cout << "  </body>\n";       // close body
            cout << "</html>\n";         // close HTML
            mysql_close(conn);           // close database connection
            return 0;                    // exit program
        }
        
        // Escape inputs to prevent SQL injection
        string safeEmail = escapeSQL(conn, email);
        string safePassword = escapeSQL(conn, password);
        
        // Check if email already exists
        {
            // Prepared statement to check existence of email
            MYSQL_STMT* stmt = mysql_stmt_init(conn);
            if (!stmt) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: Unable to initialize statement.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_close(conn);
                return 0;
            }
            const char* checkQuery = "SELECT user_id FROM users WHERE email = ? LIMIT 1";
            if (mysql_stmt_prepare(stmt, checkQuery, strlen(checkQuery))) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            MYSQL_BIND bind_param[1];
            memset(bind_param, 0, sizeof(bind_param));
            unsigned long email_len = safeEmail.length();
            bind_param[0].buffer_type = MYSQL_TYPE_STRING;
            bind_param[0].buffer = (char*)safeEmail.c_str();
            bind_param[0].buffer_length = safeEmail.length();
            bind_param[0].length = &email_len;
            if (mysql_stmt_bind_param(stmt, bind_param)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            if (mysql_stmt_execute(stmt)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            // store result to check number of rows
            if (mysql_stmt_store_result(stmt)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            my_ulonglong row_count = mysql_stmt_num_rows(stmt);
            if (row_count > 0) {  // if email already exists
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Registration Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Email already registered.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
        }
        
        // Insert new user into database
        {
            // Prepared statement for insert
            MYSQL_STMT* stmt = mysql_stmt_init(conn);
            if (!stmt) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: Unable to initialize statement.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_close(conn);
                return 0;
            }
            const char* insertQuery = "INSERT INTO users (email, password) VALUES (?, ?)";
            if (mysql_stmt_prepare(stmt, insertQuery, strlen(insertQuery))) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            MYSQL_BIND bind_param[2];
            memset(bind_param, 0, sizeof(bind_param));
            unsigned long email_len = safeEmail.length();
            unsigned long pass_len = safePassword.length();
            bind_param[0].buffer_type = MYSQL_TYPE_STRING;
            bind_param[0].buffer = (char*)safeEmail.c_str();
            bind_param[0].buffer_length = safeEmail.length();
            bind_param[0].length = &email_len;
            bind_param[1].buffer_type = MYSQL_TYPE_STRING;
            bind_param[1].buffer = (char*)safePassword.c_str();
            bind_param[1].buffer_length = safePassword.length();
            bind_param[1].length = &pass_len;
            if (mysql_stmt_bind_param(stmt, bind_param)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            if (mysql_stmt_execute(stmt)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Registration Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Registration failed: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            mysql_stmt_close(stmt);
        }
        
        // Get the new user's ID
        int newUserId = mysql_insert_id(conn);
        
        // Set session cookie and redirect to home page
        // IMPORTANT: Set cookie BEFORE Location header
        setSessionCookie(newUserId);
        cout << "Location: ./index.cgi\r\n\r\n";  // redirect to home page immediately
        
    } else if (action == "login") {
        // Handle login action
        
        // Validate input
        if (email.empty() || password.empty()) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Login Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
            cout << "  </head>\n";
            cout << "  <body>\n";
            cout << "    <div class=\"container\">\n";
            cout << "      <div class=\"message error\">Email and password are required.</div>\n";
            cout << "      <a href=\"index.cgi\">Go Back</a>\n";
            cout << "    </div>\n";
            cout << "  </body>\n";
            cout << "</html>\n";
            mysql_close(conn);
            return 0;
        }
        
        // Escape inputs to prevent SQL injection
        string safeEmail = escapeSQL(conn, email);
        string safePassword = escapeSQL(conn, password);
        
        // Query database for matching user
        {
            // Prepared statement for login
            MYSQL_STMT* stmt = mysql_stmt_init(conn);
            if (!stmt) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: Unable to initialize statement.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_close(conn);
                return 0;
            }
            const char* loginQuery = "SELECT user_id FROM users WHERE email = ? AND password = ? LIMIT 1";
            if (mysql_stmt_prepare(stmt, loginQuery, strlen(loginQuery))) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            MYSQL_BIND bind_param[2];
            memset(bind_param, 0, sizeof(bind_param));
            unsigned long email_len = safeEmail.length();
            unsigned long pass_len = safePassword.length();
            bind_param[0].buffer_type = MYSQL_TYPE_STRING;
            bind_param[0].buffer = (char*)safeEmail.c_str();
            bind_param[0].buffer_length = safeEmail.length();
            bind_param[0].length = &email_len;
            bind_param[1].buffer_type = MYSQL_TYPE_STRING;
            bind_param[1].buffer = (char*)safePassword.c_str();
            bind_param[1].buffer_length = safePassword.length();
            bind_param[1].length = &pass_len;
            if (mysql_stmt_bind_param(stmt, bind_param)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            if (mysql_stmt_execute(stmt)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            if (mysql_stmt_store_result(stmt)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            my_ulonglong row_count = mysql_stmt_num_rows(stmt);
            if (row_count == 0) {  // if no matching user found
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Login Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Invalid email or password.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            // Bind result and fetch user_id
            MYSQL_BIND bind_result[1];
            memset(bind_result, 0, sizeof(bind_result));
            int fetched_user_id = 0;
            unsigned long length_user_id = 0;
            bind_result[0].buffer_type = MYSQL_TYPE_LONG;
            bind_result[0].buffer = &fetched_user_id;
            bind_result[0].buffer_length = sizeof(fetched_user_id);
            bind_result[0].length = &length_user_id;
            if (mysql_stmt_bind_result(stmt, bind_result)) {
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Database Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Database error: " << mysql_error(conn) << "</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            if (mysql_stmt_fetch(stmt) != 0) {
                // fetch error - treat as no matching user
                cout << "Content-type: text/html\r\n\r\n";
                cout << "<!DOCTYPE html>\n";
                cout << "<html lang=\"en\">\n";
                cout << "  <head>\n";
                cout << "    <meta charset=\"UTF-8\">\n";
                cout << "    <title>Login Error</title>\n";
                cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
                cout << "  </head>\n";
                cout << "  <body>\n";
                cout << "    <div class=\"container\">\n";
                cout << "      <div class=\"message error\">Invalid email or password.</div>\n";
                cout << "      <a href=\"index.cgi\">Go Back</a>\n";
                cout << "    </div>\n";
                cout << "  </body>\n";
                cout << "</html>\n";
                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);
                mysql_close(conn);
                return 0;
            }
            int userId = fetched_user_id;       // convert user_id to integer
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            
            // Set session cookie and redirect to home page
            // IMPORTANT: Set cookie BEFORE Location header
            setSessionCookie(userId);        // create session cookie for logged in user
            cout << "Location: ./index.cgi\r\n\r\n";  // redirect to home page immediately
        }
        
    } else if (action == "logout") {
        // Handle logout action - expire the cookie and redirect
        cout << "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly\r\n";  // expire cookie by setting old date
        cout << "Location: ./index.cgi\r\n\r\n";  // redirect to home page immediately
        
    } else {
        // Unknown action - user submitted invalid action
        cout << "Content-type: text/html\r\n\r\n";  // send HTTP header
        cout << "<!DOCTYPE html>\n";     // HTML5 doctype
        cout << "<html lang=\"en\">\n";  // HTML with English language
        cout << "  <head>\n";            // head section
        cout << "    <meta charset=\"UTF-8\">\n";  // character encoding
        cout << "    <title>Error</title>\n";  // page title
        cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";  // link stylesheet (now CGI)
        cout << "  </head>\n";           // close head
        cout << "  <body>\n";            // body section
        cout << "    <div class=\"container\">\n";  // container div
        cout << "      <div class=\"message error\">Invalid action.</div>\n";  // error message
        cout << "      <a href=\"./index.cgi\">Go Back</a>\n";  // link to home
        cout << "    </div>\n";          // close container
        cout << "  </body>\n";           // close body
        cout << "</html>\n";             // close HTML
    }
    
    mysql_close(conn);                   // close database connection
    return 0;                            // exit successfully
}
