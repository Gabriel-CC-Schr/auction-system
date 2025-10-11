// style.cpp - Dynamically generates CSS stylesheet
// This file outputs CSS code that styles all pages on the website

#include <iostream>      // for input/output operations
using namespace std;

int main() {
    // Send HTTP header telling browser this is CSS content
    cout << "Content-type: text/css\r\n\r\n";
    // The content-type header tells the browser to interpret this as CSS, not HTML
    
    // Output CSS code line by line with comments
    
    cout << "/* style.css - Simple stylesheet for auction website */\n";
    // This is a CSS comment - browsers ignore it
    
    cout << "/* This file controls how the website looks */\n";
    // CSS lets us separate appearance from content
    
    cout << "\n";
    // Blank line for readability
    
    cout << "/* Reset default browser styling so all browsers look the same */\n";
    // Different browsers have different default styles, so we reset them
    
    cout << "* {\n";
    // The asterisk selects ALL elements on the page
    
    cout << "    margin: 0;\n";
    // Remove default margins (space outside elements)
    
    cout << "    padding: 0;\n";
    // Remove default padding (space inside elements)
    
    cout << "    box-sizing: border-box;\n";
    // Make width/height include padding and border
    
    cout << "}\n";
    // Close the universal selector rule
    
    cout << "\n";
    // Blank line for readability
    
    cout << "/* Style the body element which contains all page content */\n";
    // The body tag wraps all visible content
    
    cout << "body {\n";
    // Start styling the body element
    
    cout << "    font-family: Arial, sans-serif;\n";
    // Use Arial font, or any sans-serif font if Arial isn't available
    
    cout << "    background-color: #f0f0f0;\n";
    // Light gray background color for the whole page
    
    cout << "    color: #333;\n";
    // Dark gray text color for readability
    
    cout << "    padding: 20px;\n";
    // Add 20 pixels of space around all edges of the page
    
    cout << "}\n";
    // Close the body style rule
    
    cout << "\n";
    // Blank line for readability
    
    cout << "/* Style the main container that holds page content */\n";
    // The container class centers and styles our content
    
    cout << ".container {\n";
    // The dot means this is a class selector
    
    cout << "    max-width: 800px;\n";
    // Don't let the container get wider than 800 pixels
    
    cout << "    margin: 0 auto;\n";
    // Center the container horizontally (0 top/bottom, auto left/right)
    
    cout << "    background-color: white;\n";
    // White background for the content area
    
    cout << "    padding: 20px;\n";
    // Add 20 pixels of space inside the container
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a 1 pixel solid light gray border around container
    
    cout << "}\n";
    // Close the container style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style all h1 headings (main page titles) */\n";
    // h1 is used for the most important heading on each page
    
    cout << "h1 {\n";
    // Style all h1 elements
    
    cout << "    color: #333;\n";
    // Dark gray color for headings
    
    cout << "    margin-bottom: 20px;\n";
    // Add 20 pixels of space below the heading
    
    cout << "    text-align: center;\n";
    // Center the heading text
    
    cout << "}\n";
    // Close h1 style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style all h2 headings (section titles) */\n";
    // h2 is used for section headings
    
    cout << "h2 {\n";
    // Style all h2 elements
    
    cout << "    color: #333;\n";
    // Dark gray color for section headings
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below the heading
    
    cout << "    border-bottom: 1px solid #333;\n";
    // Add a thin line under the heading
    
    cout << "    padding-bottom: 5px;\n";
    // Add 5 pixels of space between text and underline
    
    cout << "}\n";
    // Close h2 style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style the navigation menu */\n";
    // The nav class styles our navigation links
    
    cout << ".nav {\n";
    // Select elements with class="nav"
    
    cout << "    text-align: center;\n";
    // Center all links in the navigation
    
    cout << "    margin-bottom: 20px;\n";
    // Add 20 pixels of space below the navigation
    
    cout << "    padding: 10px;\n";
    // Add 10 pixels of space inside the navigation
    
    cout << "    background-color: #e0e0e0;\n";
    // Light gray background for navigation area
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a light gray border around navigation
    
    cout << "}\n";
    // Close nav style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style links inside the navigation */\n";
    // This targets only links that are inside nav elements
    
    cout << ".nav a {\n";
    // Select anchor tags inside nav class
    
    cout << "    color: #0066cc;\n";
    // Blue color for links
    
    cout << "    text-decoration: none;\n";
    // Remove the underline from links
    
    cout << "    margin: 0 10px;\n";
    // Add 10 pixels of space on left and right of each link
    
    cout << "}\n";
    // Close nav link style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style each form section container */\n";
    // Form sections group related form elements
    
    cout << ".form-section {\n";
    // Select elements with class="form-section"
    
    cout << "    margin-bottom: 20px;\n";
    // Add 20 pixels of space below each form section
    
    cout << "    padding: 15px;\n";
    // Add 15 pixels of space inside each form section
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a light gray border around form section
    
    cout << "}\n";
    // Close form-section style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style all form labels */\n";
    // Labels describe what each input field is for
    
    cout << "label {\n";
    // Select all label elements
    
    cout << "    display: block;\n";
    // Make each label appear on its own line
    
    cout << "    margin-bottom: 5px;\n";
    // Add 5 pixels of space below each label
    
    cout << "}\n";
    // Close label style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style text inputs, email inputs, password inputs, and number inputs */\n";
    // These are the most common input types
    
    cout << "input[type=\"text\"],\n";
    // Select text input fields
    
    cout << "input[type=\"email\"],\n";
    // Select email input fields
    
    cout << "input[type=\"password\"],\n";
    // Select password input fields
    
    cout << "input[type=\"number\"] {\n";
    // Select number input fields
    
    cout << "    width: 100%;\n";
    // Make input fields take up full width of their container
    
    cout << "    padding: 8px;\n";
    // Add 8 pixels of space inside the input field
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below each input field
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a light gray border around input field
    
    cout << "}\n";
    // Close input style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style textarea elements (multi-line text input) */\n";
    // Textareas are used for longer text like descriptions
    
    cout << "textarea {\n";
    // Select all textarea elements
    
    cout << "    width: 100%;\n";
    // Make textarea take up full width of container
    
    cout << "    padding: 8px;\n";
    // Add 8 pixels of space inside the textarea
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below the textarea
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a light gray border around textarea
    
    cout << "    min-height: 80px;\n";
    // Set minimum height of textarea to 80 pixels
    
    cout << "}\n";
    // Close textarea style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style dropdown select elements */\n";
    // Select boxes let users choose from a dropdown list
    
    cout << "select {\n";
    // Select all select elements
    
    cout << "    width: 100%;\n";
    // Make select box take up full width of container
    
    cout << "    padding: 8px;\n";
    // Add 8 pixels of space inside the select box
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below the select box
    
    cout << "    border: 1px solid #ccc;\n";
    // Add a light gray border around select box
    
    cout << "}\n";
    // Close select style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style all buttons and elements with button class */\n";
    // Buttons can be button elements or links styled as buttons
    
    cout << "button,\n";
    // Select all button elements
    
    cout << ".button {\n";
    // Also select elements with class="button"
    
    cout << "    background-color: #0066cc;\n";
    // Blue background color for buttons
    
    cout << "    color: white;\n";
    // White text color for buttons
    
    cout << "    padding: 10px 20px;\n";
    // Add 10 pixels top/bottom, 20 pixels left/right padding
    
    cout << "    border: none;\n";
    // Remove default border from buttons
    
    cout << "    cursor: pointer;\n";
    // Show hand pointer when hovering over button
    
    cout << "    text-decoration: none;\n";
    // Remove underline if button is a link
    
    cout << "    display: inline-block;\n";
    // Allow padding to work on link buttons
    
    cout << "}\n";
    // Close button style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style all tables */\n";
    // Tables display data in rows and columns
    
    cout << "table {\n";
    // Select all table elements
    
    cout << "    width: 100%;\n";
    // Make table take up full width of container
    
    cout << "    border-collapse: collapse;\n";
    // Remove space between table cells
    
    cout << "    margin-bottom: 20px;\n";
    // Add 20 pixels of space below table
    
    cout << "}\n";
    // Close table style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style table header cells and regular cells */\n";
    // th is for headers, td is for data cells
    
    cout << "th,\n";
    // Select table header cells
    
    cout << "td {\n";
    // Also select table data cells
    
    cout << "    padding: 10px;\n";
    // Add 10 pixels of space inside each cell
    
    cout << "    text-align: left;\n";
    // Align text to the left side of cells
    
    cout << "    border-bottom: 1px solid #ccc;\n";
    // Add line between rows
    
    cout << "}\n";
    // Close th/td style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style table header cells specifically */\n";
    // Headers need different styling than data cells
    
    cout << "th {\n";
    // Select only table header cells
    
    cout << "    background-color: #333;\n";
    // Dark gray background for header row
    
    cout << "    color: white;\n";
    // White text in header row
    
    cout << "}\n";
    // Close th style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style message boxes (used for errors, success, warnings) */\n";
    // Message boxes display feedback to users
    
    cout << ".message {\n";
    // Select elements with class="message"
    
    cout << "    padding: 10px;\n";
    // Add 10 pixels of space inside message box
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below message box
    
    cout << "    border: 1px solid #ccc;\n";
    // Add light gray border around message box
    
    cout << "}\n";
    // Close message style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style error messages */\n";
    // Error messages show when something goes wrong
    
    cout << ".error {\n";
    // Select elements with class="error"
    
    cout << "    background-color: #ffcccc;\n";
    // Light red background for errors
    
    cout << "    color: #cc0000;\n";
    // Dark red text for errors
    
    cout << "    border-color: #cc0000;\n";
    // Dark red border for errors
    
    cout << "}\n";
    // Close error style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style success messages */\n";
    // Success messages show when something works correctly
    
    cout << ".success {\n";
    // Select elements with class="success"
    
    cout << "    background-color: #ccffcc;\n";
    // Light green background for success
    
    cout << "    color: #006600;\n";
    // Dark green text for success
    
    cout << "    border-color: #006600;\n";
    // Dark green border for success
    
    cout << "}\n";
    // Close success style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style warning messages */\n";
    // Warning messages show caution or important information
    
    cout << ".warning {\n";
    // Select elements with class="warning"
    
    cout << "    background-color: #ffffcc;\n";
    // Light yellow background for warnings
    
    cout << "    color: #996600;\n";
    // Dark orange text for warnings
    
    cout << "    border-color: #996600;\n";
    // Dark orange border for warnings
    
    cout << "}\n";
    // Close warning style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style auction item containers */\n";
    // Auction items are displayed in special containers
    
    cout << ".auction-item {\n";
    // Select elements with class="auction-item"
    
    cout << "    padding: 10px;\n";
    // Add 10 pixels of space inside auction item
    
    cout << "    margin-bottom: 15px;\n";
    // Add 15 pixels of space below auction item
    
    cout << "    border: 1px solid #ccc;\n";
    // Add light gray border around auction item
    
    cout << "    background-color: #f5f5f5;\n";
    // Very light gray background
    
    cout << "}\n";
    // Close auction-item style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style h3 headings inside auction items */\n";
    // h3 is used for item titles within auction items
    
    cout << ".auction-item h3 {\n";
    // Select h3 elements inside auction-item class
    
    cout << "    margin-bottom: 10px;\n";
    // Add 10 pixels of space below the heading
    
    cout << "}\n";
    // Close auction-item h3 style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style logout links specifically */\n";
    // Logout links should stand out and look different
    
    cout << ".logout {\n";
    // Select elements with class="logout"
    
    cout << "    color: #cc0000;\n";
    // Red color for logout link to indicate action
    
    cout << "}\n";
    // Close logout style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style source code box */\n";
    // The source code box shows links to C++ files
    
    cout << ".source-box {\n";
    // Select elements with class="source-box"
    
    cout << "    max-width: 800px;\n";
    // Don't let box get wider than 800 pixels
    
    cout << "    margin: 0 auto 20px;\n";
    // Center horizontally and add 20px space below
    
    cout << "    background-color: white;\n";
    // White background
    
    cout << "    padding: 15px;\n";
    // Add 15 pixels of space inside box
    
    cout << "    border: 1px solid #ccc;\n";
    // Add light gray border
    
    cout << "}\n";
    // Close source-box style rule
    
    cout << "\n";
    // Blank line
    
    cout << "/* Style links inside source box */\n";
    // Links to source code files need special styling
    
    cout << ".source-box a {\n";
    // Select anchor tags inside source-box class
    
    cout << "    display: block;\n";
    // Make each link appear on its own line
    
    cout << "    color: #0066cc;\n";
    // Blue color for links
    
    cout << "    text-decoration: none;\n";
    // Remove underline
    
    cout << "    padding: 5px;\n";
    // Add 5 pixels of space around each link
    
    cout << "}\n";
    // Close source-box link style rule
    
    return 0;
    // Exit successfully
}