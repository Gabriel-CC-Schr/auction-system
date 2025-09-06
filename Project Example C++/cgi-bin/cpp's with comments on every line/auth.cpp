// auth.cpp — Authentication + Registration CGI with MariaDB (every line commented)

#include <bits/stdc++.h>                 // Include standard C++ headers (strings, streams, containers, etc.)
#include <mariadb/mysql.h>               // Include MariaDB/MySQL C client API for database access
using namespace std;                     // Use the standard namespace to avoid std:: prefixes

#define DB_HOST "localhost"              // Database host (localhost matches root@localhost or your created user)
#define DB_USER "root"                   // Database username (change to a dedicated user in production)
#define DB_PASS ""                       // Database password (empty for XAMPP default root; set a real one later)
#define DB_NAME "auctiondb"              // Target database name created by your DDL
#define DB_PORT 3306                     // Default MySQL/MariaDB TCP port

// ---------- Utility: URL decode application/x-www-form-urlencoded ----------
static string url_decode(const string& s){               // Decode %XX sequences and '+' into spaces
    string out; out.reserve(s.size());                   // Reserve capacity to minimize reallocations
    for(size_t i=0;i<s.size();++i){                      // Loop over each character in the input
        if(s[i]=='+') out.push_back(' ');               // '+' encodes a space in form-encoded bodies
        else if(s[i]=='%' && i+2<s.size()){             // '%' introduces a two-hex-digit encoded byte
            int v=0; sscanf(s.substr(i+1,2).c_str(), "%x", &v); // Parse next two chars as hex into int v
            out.push_back(static_cast<char>(v));        // Append the decoded byte to the output
            i+=2;                                       // Skip the two hex characters we just consumed
        } else out.push_back(s[i]);                     // Otherwise copy the character as-is
    }
    return out;                                         // Return the fully decoded string
}

// ---------- Utility: Parse key=value&key2=value2 into a map ----------
static map<string,string> parse_kv(const string& s){     // Convert a URL-encoded query/body to a map
    map<string,string> m;                                // Result container for key/value pairs
    size_t i=0;                                          // Cursor into the string
    while(i<s.size()){                                   // Continue until the end of the string
        size_t e = s.find('=', i);                       // Find '=' between key and value
        if(e==string::npos) break;                       // If no '=', stop parsing
        size_t a = s.find('&', e+1);                     // Find '&' that ends this pair (or npos)
        string k = url_decode(s.substr(i, e-i));         // Decode key portion
        string v = url_decode(s.substr(e+1, (a==string::npos?s.size():a)-(e+1))); // Decode value portion
        m[k]=v;                                          // Store parsed key/value
        if(a==string::npos) break;                       // If there was no '&', this was the last pair
        i = a+1;                                         // Move cursor to the start of the next pair
    }
    return m;                                            // Return the populated map
}

// ---------- Utility: Escape HTML for safe output ----------
static string html_escape(const string& s){              // Replace HTML special chars with entities
    string o; o.reserve(s.size());                       // Reserve capacity for performance
    for(char c : s){                                     // Iterate characters
        switch(c){                                       // Branch on each char
            case '&': o += "&amp;";  break;              // Escape '&'
            case '<': o += "&lt;";   break;              // Escape '<'
            case '>': o += "&gt;";   break;              // Escape '>'
            case '"': o += "&quot;"; break;              // Escape '"'
            case '\'':o += "&#39;";  break;              // Escape '\''
            default:   o.push_back(c);                   // Keep everything else unchanged
        }
    }
    return o;                                            // Return escaped string
}

// ---------- Utility: Random hex string for salts / session tokens ----------
static string rand_hex(size_t nbytes){                   // Generate nbytes of randomness and print as hex
    static random_device rd;                             // Non-deterministic seed for PRNG
    static mt19937_64 gen(rd());                         // 64-bit Mersenne Twister PRNG
    uniform_int_distribution<int> dist(0,255);           // Uniform byte distribution
    string s; s.reserve(nbytes*2);                       // Reserve two characters per byte
    char buf[3];                                         // Temporary buffer for "%02x"
    for(size_t i=0;i<nbytes;i++){                        // Generate each random byte
        sprintf(buf, "%02x", dist(gen));                 // Format the byte as two lowercase hex digits
        s += buf;                                        // Append to the result string
    }
    return s;                                            // Return the final hex string
}

// ---------- DB: Connect to MariaDB with option-files ignored and TLS disabled ----------
static MYSQL* db_connect(){                              // Return a connected MYSQL* or nullptr on failure
    MYSQL* conn = mysql_init(nullptr);                   // Allocate and initialize a client handle
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4"); // Request utf8mb4 charset for the session
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF) // If using MariaDB Connector/C with this option
    { unsigned int nocnf = 1; mysql_options(conn, MARIADB_OPT_NOCONF, &nocnf); } // Ignore option files (my.ini)
#endif                                                  // End conditional for MARIADB_OPT_NOCONF
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE) // If library supports SSL enforce toggle
    { unsigned int enforce = 0; mysql_options(conn, MARIADB_OPT_SSL_ENFORCE, &enforce); } // Do NOT enforce TLS
#endif                                                  // End conditional for MARIADB_OPT_SSL_ENFORCE
#if defined(MYSQL_OPT_SSL_MODE)                          // If building against libmysql with SSL modes
#ifndef SSL_MODE_DISABLED                                // Ensure SSL_MODE_DISABLED is defined
#define SSL_MODE_DISABLED 0                              // Define disabled mode value if missing
#endif                                                   // End define guard for SSL_MODE_DISABLED
    { int mode = SSL_MODE_DISABLED; mysql_options(conn, MYSQL_OPT_SSL_MODE, &mode); } // Explicitly disable TLS
#endif                                                  // End conditional for MYSQL_OPT_SSL_MODE
    if(!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr; // Attempt connect
    return conn;                                         // Return the connected handle on success
}

// ---------- DB: Escape user input for safe SQL literals ----------
static string sql_escape(MYSQL* c, const string& in){    // Escape arbitrary string for SQL literal context
    string out; out.resize(in.size()*2+1);               // Reserve worst-case size (every byte escaped)
    unsigned long n = mysql_real_escape_string(c, out.data(), in.c_str(), in.size()); // Perform the escape
    out.resize(n);                                       // Shrink to the actual escaped length
    return out;                                          // Return the escaped string
}

// ---------- CGI helpers: headers and page framing ----------
static void print_header(const string& extra=""){        // Emit HTTP headers; allow extra header lines
    cout << "Content-Type: text/html\r\n";              // Output the Content-Type header for HTML
    if(!extra.empty()) cout << extra;                   // If caller supplied extra headers, print them verbatim
    cout << "\r\n";                                     // Blank line that ends the HTTP header section
}
static void page_top(const string& title){               // Begin HTML document with minimal styles
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>"                      // HTML5 doctype + head start
         << html_escape(title)                                                               // Safe page title
         << "</title><style>body{font-family:sans-serif;max-width:900px;margin:24px auto;padding:0 12px}" // Body styling
            "form{margin:16px 0}input,button,select,textarea{padding:8px;margin:4px 0}"     // Form control spacing
            "a.button,button{border:1px solid #ccc;border-radius:8px;padding:8px 12px;text-decoration:none;display:inline-block}"
         << "</style></head><body>";                                                         // Close head, open body
    cout << "<h1>" << html_escape(title) << "</h1>";                                         // Page H1 header
}
static void page_bottom(){ cout << "</body></html>"; }   // Close the HTML document cleanly

// ---------- Forms: login + registration ----------
static void show_forms(const string& msg=""){            // Render the two forms plus optional message
    if(!msg.empty()) cout << "<p style='color:#b00'><strong>" << html_escape(msg) << "</strong></p>"; // Show error/info
    cout << "<h2>Log in</h2>";                          // Login section header
    cout << "<form method='POST'><input type='hidden' name='action' value='login'>"          // Begin login form
            "Email: <input type='email' name='email' required><br>"                         // Email input
            "Password: <input type='password' name='password' required><br>"               // Password input
            "<button type='submit'>Log in</button></form>";                                 // Submit button
    cout << "<h2>Register</h2>";                        // Registration section header
    cout << "<form method='POST'><input type='hidden' name='action' value='register'>"      // Begin registration form
            "Email: <input type='email' name='email' required><br>"                         // Email field
            "Password: <input type='password' name='password' required><br>"               // Password field
            "<button type='submit'>Create account</button></form>";                         // Submit button
    cout << "<hr><p>After logging in, try: "                                                // Quick navigation links
            "<a class='button' href='/cgi-bin/listings.cgi'>All open auctions</a> · "      // Link to listings
            "<a class='button' href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · "             // Link to bid/sell
            "<a class='button' href='/cgi-bin/transactions.cgi'>Your transactions</a></p>";// Link to transactions
}

// ---------- POST handler (register/login). Emits headers only after we know Set-Cookie needs to be sent ----------
static void handle_post(){                               // Process POST requests for login/register
    const char* cl = getenv("CONTENT_LENGTH");           // Read CONTENT_LENGTH to know how many bytes to read
    size_t n = cl ? strtoul(cl, nullptr, 10) : 0;        // Convert CONTENT_LENGTH to size_t (or 0 if missing)
    string body; body.resize(n);                         // Allocate buffer for the request body
    if(n) fread(body.data(), 1, n, stdin);               // Read exactly n bytes from stdin (the POST data)
    auto kv = parse_kv(body);                            // Parse form body into a key/value map
    string action = kv.count("action")?kv["action"]:"";  // Extract "action" (login or register)
    string email  = kv.count("email") ?kv["email"] :"";  // Extract email field (may be empty)
    string pass   = kv.count("password")?kv["password"]:""; // Extract password field (may be empty)

    string extra_headers;                                // Buffer for any extra HTTP headers (e.g., Set-Cookie)
    string page_msg;                                     // Message to show at top of the page
    bool   done_html=false;                              // Track if we already printed page content (safety)

    if(email.empty() || pass.empty() || (action!="register" && action!="login")){ // Validate basic inputs
        print_header();                                  // Emit standard headers (no Set-Cookie)
        page_top("Auction Portal — Auth");               // Open page
        show_forms("Please fill in all fields.");        // Show validation error and forms
        page_bottom();                                   // Close page
        return;                                          // Stop processing
    }

    MYSQL* db = db_connect();                            // Connect to the database
    if(!db){                                             // If connection failed
        print_header();                                  // Emit headers (no cookie)
        page_top("Auction Portal — Auth");               // Open page
        show_forms("DB connection failed.");             // Show DB error and forms
        page_bottom();                                   // Close page
        return;                                          // Stop processing
    }

    string em = sql_escape(db, email);                   // Escape email for SQL
    string pw = sql_escape(db, pass);                    // Escape password for SQL (used inside server-side hash)

    if(action=="register"){                              // Handle account creation
        string q = "SELECT user_id FROM users WHERE email='"+em+"' LIMIT 1"; // Check if email is already taken
        if(mysql_query(db, q.c_str())==0){               // Execute the SELECT
            MYSQL_RES* r = mysql_store_result(db);       // Retrieve result set
            bool exists = r && mysql_num_rows(r)>0;      // Determine if any row exists
            if(r) mysql_free_result(r);                  // Free result memory
            if(exists){                                  // If email exists, show error
                print_header();                          // Emit headers
                page_top("Auction Portal — Auth");       // Open page
                show_forms("Email already registered."); // Inform user
                page_bottom();                           // Close page
                mysql_close(db);                         // Close DB connection
                return;                                  // Stop processing
            }
        }
        string salt = rand_hex(16);                      // Generate a per-user random salt
        string ins = "INSERT INTO users(email,password_salt,password_hash) VALUES('"+em+"','"+salt+"',"
                     "SHA2(CONCAT('"+salt+"','"+pw+"'),256))"; // Build INSERT using server-side SHA2(salt||password)
        if(mysql_query(db, ins.c_str())!=0){             // Execute the INSERT
            print_header();                              // Emit headers
            page_top("Auction Portal — Auth");           // Open page
            show_forms("Registration failed.");          // Show generic failure
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        }
        my_ulonglong uid = mysql_insert_id(db);          // Capture the new user_id
        string tok = rand_hex(32);                       // Create a session token
        string sess = "INSERT INTO sessions(session_id,user_id,expires_at) VALUES('"+tok+"',"
                      +to_string((unsigned long long)uid)+", DATE_ADD(NOW(), INTERVAL 7 DAY))"; // Insert session row
        if(mysql_query(db, sess.c_str())==0){            // If session was created successfully
            extra_headers = "Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax\r\n"; // Prepare Set-Cookie header
            print_header(extra_headers);                 // Emit headers including Set-Cookie
            page_top("Auction Portal — Auth");           // Open page
            cout << "<p>Registered! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>"; // Success message/link
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        } else {                                         // If session insert failed
            print_header();                              // Emit headers
            page_top("Auction Portal — Auth");           // Open page
            show_forms("Could not create session, try login."); // Ask user to log in manually
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        }
    }

    if(action=="login"){                                 // Handle login flow
        string q = "SELECT user_id FROM users WHERE email='"+em+"' "
                   "AND password_hash = SHA2(CONCAT(password_salt,'"+pw+"'),256) LIMIT 1"; // Verify salted hash
        if(mysql_query(db, q.c_str())!=0){               // Execute the credential check
            print_header();                              // Emit headers
            page_top("Auction Portal — Auth");           // Open page
            show_forms("Login query failed.");           // Report query failure
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        }
        MYSQL_RES* r = mysql_store_result(db);           // Retrieve the result set
        if(!r || mysql_num_rows(r)==0){                  // No matching user/password found
            if(r) mysql_free_result(r);                  // Free result if allocated
            print_header();                              // Emit headers
            page_top("Auction Portal — Auth");           // Open page
            show_forms("Invalid credentials.");          // Show invalid credentials message
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        }
        MYSQL_ROW row = mysql_fetch_row(r);              // Fetch the single row
        unsigned long long uid = strtoull(row[0], nullptr, 10); // Parse user_id from text to integer
        mysql_free_result(r);                            // Free result set
        string tok = rand_hex(32);                       // Create a new session token
        string sess = "INSERT INTO sessions(session_id,user_id,expires_at) VALUES('"+tok+"',"
                      +to_string(uid)+", DATE_ADD(NOW(), INTERVAL 7 DAY))"; // Build insert for session
        if(mysql_query(db, sess.c_str())==0){            // Execute the session insert
            extra_headers = "Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax\r\n"; // Prepare cookie header
            print_header(extra_headers);                 // Emit headers including Set-Cookie
            page_top("Auction Portal — Auth");           // Open page
            cout << "<p>Logged in! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>"; // Success link
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        } else {                                         // If session creation failed
            print_header();                              // Emit headers
            page_top("Auction Portal — Auth");           // Open page
            show_forms("Could not start session.");      // Let the user know session failed
            page_bottom();                               // Close page
            mysql_close(db);                             // Close DB
            return;                                      // Stop processing
        }
    }

    // If we got here, action was something unexpected (shouldn't happen with earlier validation).
    print_header();                                      // Emit headers for a generic response
    page_top("Auction Portal — Auth");                  // Open page
    show_forms("Unknown action.");                       // Inform about unknown action
    page_bottom();                                       // Close page
    mysql_close(db);                                     // Close DB just in case
    return;                                              // Finish handler
}

// ---------- CGI entry point ----------
int main(){                                              // Program entry called by the web server (CGI)
    const char* rm = getenv("REQUEST_METHOD");           // Read HTTP method (GET/POST) from environment
    if(!rm || string(rm)=="GET"){                        // If GET or method missing, show forms
        print_header();                                  // Emit standard headers
        page_top("Auction Portal — Auth");               // Start page
        show_forms();                                    // Render login/registration forms
        page_bottom();                                   // Close page
    } else {                                             // Otherwise treat as POST (form submission)
        handle_post();                                   // Process login/registration and respond
    }
    return 0;                                            // Return success to the web server
}
