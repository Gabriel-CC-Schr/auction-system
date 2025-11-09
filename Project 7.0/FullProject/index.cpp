//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: index.cpp
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

    // grab query strings from stuff in URL after the "?"
    // need this to show error or success messages
    const char* queryString = getenv("QUERY_STRING");
    string qs = queryString ? queryString : "";
    
    // connect to database: if fails cant check sessions or do much else
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                           DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0)) {
        // We'll print a basic error page so the user sees something helpful.
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<html><body><h1>Database Connection Error</h1></body></html>\n";
        return 1;
    }
    
    // figure out the session state (logged in, expired, or none)
    int userId = 0;
    int sessionState = getSessionState(conn, userId);
    
    // if session expired, clear cookie so browser doesn't think it's still valid
    if (sessionState == SESSION_EXPIRED) {
        clearSessionCookie();
    }

    // prepare little status message shown at top-left of page
    string statusMessage;
    if (sessionState == SESSION_LOGGED_IN) {
        statusMessage = "you are currently logged in";
    } else if (sessionState == SESSION_EXPIRED) {
        statusMessage = "logged out due to inactivity";
    } else {
        statusMessage = "not logged in yet";
    }

    // start sending our HTML page back to browser
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "  <head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>All-In Dragons Auctions - Home</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "  </head>\n";
    cout << "  <body>\n";
    
    // little status message in top-left that tells user their login status
    cout << "    <div class=\"status\"><em>" << htmlEscape(statusMessage) << "</em></div>\n";
    
    // main content container: just a white box with stuff in it
    cout << "    <div class=\"container\">\n";
    cout << "      <h1>All-In Dragons Auctions</h1>\n";

	// listings link available to anyone (no login or registration needed)
	cout << "      <div class=\"nav\" style=\"text-align: center;\">\n";
	cout << "        <a href=\"./listings.cgi\">View Our Listings (Coming Soon)</a>\n";
	cout << "      </div>\n";
    
    // check URL for ?error= or ?success= to show friendly messages
    string error = getValue(qs, "error");
    string success = getValue(qs, "success");
    
    // if error code, show correct message box
    if (!error.empty()) {
        cout << "      <div class=\"error\">\n";
        if (error == "email_exists") {
            cout << "        Email already registered. Please login or use a different email.\n";
        } else if (error == "invalid") {
            cout << "        Invalid email or password. Please try again.\n";
        } else if (error == "database") {
            cout << "        Database error. Please try again later.\n";
        } else if (error == "session") {
            cout << "        Session creation failed. Please try again.\n";
        } else {
            // escape out any weird text before showing it just to be safe
            cout << "        " << htmlEscape(error) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // show success message
    if (!success.empty()) {
        cout << "      <div class=\"success\">\n";
        if (success == "registered") {
            cout << "        Registration successful! Please login below.\n";
        } else {
            cout << "        " << htmlEscape(success) << "\n";
        }
        cout << "      </div>\n";
    }
    
    // if user already logged in, we don't show forms.
    // for now we show links to pages that are coming soon
    if (sessionState == SESSION_LOGGED_IN) {
        cout << "      <p>You have successfully logged in.</p>\n";
        cout << "      <div class=\"nav\">\n";
        cout << "        <a href=\"listings.cgi\">Listings (coming soon)</a>\n";
        cout << "        <a href=\"trade.cgi\">Buy or Sell</a>\n";
        cout << "        <a href=\"transactions.cgi\">My Transactions</a>\n";
        cout << "        <a href=\"login.cgi?action=logout\">Logout</a>\n";
        cout << "      </div>\n";
    } else {
        // login box and forms: where users log in
        cout << "      <h2>Login</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"login\">\n";
        cout << "        <label for=\"login_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"login_email\" name=\"email\" required>\n";
        cout << "        <label for=\"login_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"login_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Login</button>\n";
        cout << "      </form>\n";
        
        // registration box and forms: for users to make new accounts
        cout << "      <h2>Register New Account</h2>\n";
        cout << "      <form action=\"login.cgi\" method=\"post\">\n";
        cout << "        <input type=\"hidden\" name=\"action\" value=\"register\">\n";
        cout << "        <label for=\"register_email\">Email:</label>\n";
        cout << "        <input type=\"email\" id=\"register_email\" name=\"email\" required>\n";
        cout << "        <label for=\"register_password\">Password:</label>\n";
        cout << "        <input type=\"password\" id=\"register_password\" name=\"password\" required>\n";
        cout << "        <button type=\"submit\">Register</button>\n";
        cout << "      </form>\n";
    }
    
    // close up main container and page
    cout << "    </div>\n";
    cout << "  </body>\n";
    cout << "</html>\n";
    
    
    mysql_close(conn);

    return 0;
}