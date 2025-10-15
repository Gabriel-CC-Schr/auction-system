// index.cpp - Main login and registration page
// This file dynamically generates the HTML for the home page

#include <iostream>      // for input/output operations
using namespace std;

int main() {
    // Send HTTP header telling browser this is HTML content
    cout << "Content-type: text/html\r\n\r\n";
    
    // Start outputting HTML5 compliant page
    cout << "<!DOCTYPE html>\n";
    // This tells the browser we are using HTML5
    
    cout << "<html lang=\"en\">\n";
    // The lang attribute tells browsers and screen readers this page is in English
    
    cout << "<head>\n";
    // The head section contains information about the page
    
    cout << "    <meta charset=\"UTF-8\">\n";
    // This sets the character encoding to UTF-8 which supports all languages
    
    cout << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    // This makes the page work well on mobile devices by setting the viewport
    
    cout << "    <title>Auction Website - Login or Register</title>\n";
    // This is the title that appears in the browser tab
    
    cout << "    <link rel=\"stylesheet\" href=\"style.cgi\">\n";
    // This links to our CSS stylesheet (now a CGI file)
    
    cout << "</head>\n";
    // Close the head section
    
    cout << "<body>\n";
    // The body section contains all visible content on the page
    
    
    // Main container holds all the main content of the page
    cout << "    <div class=\"container\">\n";
    // This div is styled by our CSS to look nice and centered
    
    cout << "        <h1>Welcome to the Auction Site</h1>\n";
    // Main heading welcoming users to the site
    
    // Login form section
    cout << "        <div class=\"form-section\">\n";
    // This div groups the login form together
    
    cout << "            <h2>Login</h2>\n";
    // Heading for the login section
    
    cout << "            <form action=\"./login.cgi\" method=\"POST\">\n";
    // Form that sends login data to login.cgi using POST method
    // POST method hides the data from the URL for security
    
    cout << "                <input type=\"hidden\" name=\"action\" value=\"login\">\n";
    // Hidden field that tells login.cgi this is a login request
    // Users don't see this field but it gets sent with the form
    
    cout << "                <label for=\"login-email\">Email:</label>\n";
    // Label for the email input field
    // The "for" attribute connects this label to the input below
    
    cout << "                <input type=\"email\" id=\"login-email\" name=\"email\" required>\n";
    // Email input field with validation
    // id="login-email" connects to the label above
    // name="email" is how we identify this data when it's sent
    // required means the form won't submit without this field filled
    
    cout << "                <label for=\"login-password\">Password:</label>\n";
    // Label for the password input field
    
    cout << "                <input type=\"password\" id=\"login-password\" name=\"password\" required>\n";
    // Password input field that hides what you type
    // type="password" makes the text show as dots for security
    
    cout << "                <button type=\"submit\">Login</button>\n";
    // Button that submits the login form when clicked
    
    cout << "            </form>\n";
    // Close the login form
    
    cout << "        </div>\n";
    // Close the login form section div
    
    // Registration form section
    cout << "        <div class=\"form-section\">\n";
    // This div groups the registration form together
    
    cout << "            <h2>Register New Account</h2>\n";
    // Heading for the registration section
    
    cout << "            <form action=\"./login.cgi\" method=\"POST\">\n";
    // Form that sends registration data to login.cgi using POST
    
    cout << "                <input type=\"hidden\" name=\"action\" value=\"register\">\n";
    // Hidden field that tells login.cgi this is a registration request
    
    cout << "                <label for=\"register-email\">Email:</label>\n";
    // Label for the registration email input
    
    cout << "                <input type=\"email\" id=\"register-email\" name=\"email\" required>\n";
    // Email input for registration
    // Browser will check that this looks like a valid email
    
    cout << "                <label for=\"register-password\">Password:</label>\n";
    // Label for the registration password input
    
    cout << "                <input type=\"password\" id=\"register-password\" name=\"password\" required>\n";
    // Password input for registration
    
    cout << "                <label for=\"confirm-password\">Confirm Password:</label>\n";
    // Label for the password confirmation input
    
    cout << "                <input type=\"password\" id=\"confirm-password\" name=\"confirm_password\" required>\n";
    // Password confirmation field to prevent typos
    // The login.cgi program will check that both passwords match
    
    cout << "                <button type=\"submit\">Register</button>\n";
    // Button that submits the registration form when clicked
    
    cout << "            </form>\n";
    // Close the registration form
    
    cout << "        </div>\n";
    // Close the registration form section div
    
    cout << "    </div>\n";
    // End of main container div
    
    cout << "</body>\n";
    // End of body section
    
    cout << "</html>\n";
    // End of HTML document
    
    return 0;
    // Exit successfully
}