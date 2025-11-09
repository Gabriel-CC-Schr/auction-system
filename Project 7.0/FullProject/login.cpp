//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: login.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

#include <iostream>
#include <string>
#include <cstdlib>
#include <mysql/mysql.h>
#include "common.cpp"
using namespace std;

int main() {

    // INFO REQUESTS
    // REQUEST_METHOD tells us if browser did GET or POST
    // QUERY_STRING holds stuff after "?" in the URL (like action=logout)
    const char* requestMethod = getenv("REQUEST_METHOD");
    const char* queryString = getenv("QUERY_STRING");
    
    //  connect to the database. If fails, can't do anything else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // Minimal error page so the user sees something.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // figure out session situation at the very start
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // If logged in and active keep session alive by updating last_activity.
    if (sessionState == SESSION_LOGGED_IN) {
        string sessionId = getCookie("session");
        renewSessionActivity(conn, sessionId);
    }
    
    // If session timed out clear browser cookie so user is "logged out".
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }
    
    // handle logout action from URL ?action=logout
    if (queryString && string(queryString).find("action=logout") != string::npos) {
        string sessionId = getCookie("session");
        if (!sessionId.empty()) {
            destroySession(conn, sessionId);   // remove from DB
        }
        clearSessionCookie();                   // remove from browser
        cout << "Location: ./index.cgi\r\n\r\n"; // send them back home
        mysql_close(conn);
        return 0;
    }
    
    // handle POST form submissions either register or login
    if (requestMethod && string(requestMethod) == "POST") {
        // The browser tells us how many bytes are in the POST body.
        const char* contentLength = getenv("CONTENT_LENGTH");
        int length = contentLength ? atoi(contentLength) : 0;
        
        // read exactly that many bytes from cin
        string postData;
        if (length > 0) {
            char* buffer = new char[length + 1];
            cin.read(buffer, length);
            buffer[length] = '\0';
            postData = buffer;
            delete[] buffer;
        }
        
        // pull form fields out of the POST body.
        string action = getValue(postData, "action");     // "register" or "login"
        string email = getValue(postData, "email");
        string password = getValue(postData, "password");
        
        // sql injection prevention:
        // escape email and password before using in SQL 
        string escapedEmail = escapeSQL(conn, email);
        string escapedPassword = escapeSQL(conn, password);
        
        // REGISTERING NEW USERS:
        // insert a new row into "users" using email and password typed
        if (action == "register") {
            string query = "INSERT INTO users (email, password) VALUES ('" + 
                          escapedEmail + "', '" + escapedPassword + "')";
            
            if (mysql_query(conn, query.c_str())) {
                // duplicate email send message to user
                cout << "Location: ./index.cgi?error=email_exists\r\n\r\n";
            } else {
                // registered successfully, tell user to log in
                cout << "Location: ./index.cgi?success=registered\r\n\r\n";
            }
        } else if (action == "login") {
            // look up email and password in users table and upon a match
            // create a new session and set cookie          
            string query = "SELECT user_id FROM users WHERE email = '" + 
                          escapedEmail + "' AND password = '" + escapedPassword + "'";
            
            // database problems: send user a basic error
            if (mysql_query(conn, query.c_str())) {
                cout << "Location: ./index.cgi?error=database\r\n\r\n";
            } else {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);
                
                // we got a user! make session set cookie
                if (row) {
                    int loggedInUserId = atoi(row[0]);
                    string sessionId = createSession(conn, loggedInUserId);

                    
                    if (!sessionId.empty()) {
                        // IMPORTANT: cookie header must come BEFORE Location redirect
                        cout << "Set-Cookie: session=" << sessionId << "; HttpOnly\r\n";
                        cout << "Location: ./index.cgi\r\n\r\n";
                    } else {
                        // couldn't make session for some reason send user a basic error
                        cout << "Location: ./index.cgi?error=session\r\n\r\n";
                    }
                } else {
                    // no matching user send wrong email or password message
                    cout << "Location: ./index.cgi?error=invalid\r\n\r\n";
                }
                
                mysql_free_result(result);
            }
        } else {
            // if anything else happens (weird) send user home
            cout << "Location: ./index.cgi\r\n\r\n";
        }
        
        mysql_close(conn);
        return 0;
    }
    
    // DIRECTLY ACCESSING LOGIN.CGI:
    // this basic page shows up when directly accessing login.cgi
    // whereas the actual form is on index.cgi
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>Authentication Handler</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>Authentication Handler</h1>\n";
    cout << "      <p>This page handles login and registration.</p>\n";
    
    // if the user already logged in we show a quick logout form.
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>You are currently logged in.</p>\n";
        cout << "      <form action=\"login.cgi\" method=\"get\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"logout\">\n";
        cout << "        <button type=\"submit\" class=\"logout-btn\">Logout</button>\n";
        cout << "      </form>\n";
    }
    
    cout << "      <div class=\"nav\">\n";
    cout << "        <a href=\"index.cgi\">Return to Home</a>\n";
    cout << "      </div>\n";
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
   
    mysql_close(conn);

    return 0;
}