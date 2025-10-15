// index.cpp - Main login and registration page

#include <iostream>
using namespace std;

int main() {
    cout << "Content-type: text/html\r\n\r\n";
    
    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    cout << "<head>\n";
    cout << "    <meta charset=\"UTF-8\">\n";
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    cout << "    <title>Auction Website - Login or Register</title>\n";
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    cout << "</head>\n";
    cout << "<body>\n";
        
    cout << "    <div class=\"container\">\n";
    cout << "        <h1>Welcome to the Auction Site</h1>\n";
    
    cout << "        <div class=\"form-section\">\n";
    cout << "            <h2>Login</h2>\n";
    cout << "            <form action=\"./login.cgi\" method=\"POST\">\n";
    cout << "                <input type=\"hidden\" name=\"action\" value=\"login\">\n";
    cout << "                <label for=\"login-email\">Email:</label>\n";
    cout << "                <input type=\"email\" id=\"login-email\" name=\"email\" required>\n";
    cout << "                <label for=\"login-password\">Password:</label>\n";
    cout << "                <input type=\"password\" id=\"login-password\" name=\"password\" required>\n";
    cout << "                <button type=\"submit\">Login</button>\n";
    cout << "            </form>\n";
    cout << "        </div>\n";
    
    cout << "        <div class=\"form-section\">\n";
    cout << "            <h2>Register New Account</h2>\n";
    cout << "            <form action=\"./login.cgi\" method=\"POST\">\n";
    cout << "                <input type=\"hidden\" name=\"action\" value=\"register\">\n";
    cout << "                <label for=\"register-email\">Email:</label>\n";
    cout << "                <input type=\"email\" id=\"register-email\" name=\"email\" required>\n";
    cout << "                <label for=\"register-password\">Password:</label>\n";
    cout << "                <input type=\"password\" id=\"register-password\" name=\"password\" required>\n";
    cout << "                <label for=\"confirm-password\">Confirm Password:</label>\n";
    cout << "                <input type=\"password\" id=\"confirm-password\" name=\"confirm_password\" required>\n";
    cout << "                <button type=\"submit\">Register</button>\n";
    cout << "            </form>\n";
    cout << "        </div>\n";
    
    cout << "    </div>\n";
    cout << "</body>\n";
    cout << "</html>\n";
    
    return 0;
}