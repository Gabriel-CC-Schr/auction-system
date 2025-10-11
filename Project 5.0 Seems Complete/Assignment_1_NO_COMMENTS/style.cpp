// style.cpp - Dynamically generates CSS stylesheet

#include <iostream>
using namespace std;

int main() {
    cout << "Content-type: text/css\r\n\r\n";
    
    cout << "* {\n";
    cout << "    margin: 0;\n";
    cout << "    padding: 0;\n";
    cout << "    box-sizing: border-box;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "body {\n";
    cout << "    font-family: Arial, sans-serif;\n";
    cout << "    background-color: #f0f0f0;\n";
    cout << "    color: #333;\n";
    cout << "    padding: 20px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".container {\n";
    cout << "    max-width: 800px;\n";
    cout << "    margin: 0 auto;\n";
    cout << "    background-color: white;\n";
    cout << "    padding: 20px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "h1 {\n";
    cout << "    color: #333;\n";
    cout << "    margin-bottom: 20px;\n";
    cout << "    text-align: center;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "h2 {\n";
    cout << "    color: #333;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border-bottom: 1px solid #333;\n";
    cout << "    padding-bottom: 5px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".nav {\n";
    cout << "    text-align: center;\n";
    cout << "    margin-bottom: 20px;\n";
    cout << "    padding: 10px;\n";
    cout << "    background-color: #e0e0e0;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".nav a {\n";
    cout << "    color: #0066cc;\n";
    cout << "    text-decoration: none;\n";
    cout << "    margin: 0 10px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".nav a:hover {\n";
    cout << "    text-decoration: underline;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".form-section {\n";
    cout << "    margin-bottom: 20px;\n";
    cout << "    padding: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "label {\n";
    cout << "    display: block;\n";
    cout << "    margin-bottom: 5px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "input[type=\"text\"],\n";
    cout << "input[type=\"email\"],\n";
    cout << "input[type=\"password\"],\n";
    cout << "input[type=\"number\"] {\n";
    cout << "    width: 100%;\n";
    cout << "    padding: 8px;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "textarea {\n";
    cout << "    width: 100%;\n";
    cout << "    padding: 8px;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "    min-height: 80px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "select {\n";
    cout << "    width: 100%;\n";
    cout << "    padding: 8px;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "button,\n";
    cout << ".button {\n";
    cout << "    background-color: #0066cc;\n";
    cout << "    color: white;\n";
    cout << "    padding: 10px 20px;\n";
    cout << "    border: none;\n";
    cout << "    cursor: pointer;\n";
    cout << "    text-decoration: none;\n";
    cout << "    display: inline-block;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "button:hover,\n";
    cout << ".button:hover {\n";
    cout << "    background-color: #0052a3;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "table {\n";
    cout << "    width: 100%;\n";
    cout << "    border-collapse: collapse;\n";
    cout << "    margin-bottom: 20px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "th,\n";
    cout << "td {\n";
    cout << "    padding: 10px;\n";
    cout << "    text-align: left;\n";
    cout << "    border-bottom: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "th {\n";
    cout << "    background-color: #333;\n";
    cout << "    color: white;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << "tr:hover {\n";
    cout << "    background-color: #f5f5f5;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".message {\n";
    cout << "    padding: 10px;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".error {\n";
    cout << "    background-color: #ffcccc;\n";
    cout << "    color: #cc0000;\n";
    cout << "    border-color: #cc0000;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".success {\n";
    cout << "    background-color: #ccffcc;\n";
    cout << "    color: #006600;\n";
    cout << "    border-color: #006600;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".warning {\n";
    cout << "    background-color: #ffffcc;\n";
    cout << "    color: #996600;\n";
    cout << "    border-color: #996600;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".auction-item {\n";
    cout << "    padding: 10px;\n";
    cout << "    margin-bottom: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "    background-color: #f5f5f5;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".auction-item h3 {\n";
    cout << "    margin-bottom: 10px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".logout {\n";
    cout << "    color: #cc0000;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".source-box {\n";
    cout << "    max-width: 800px;\n";
    cout << "    margin: 0 auto 20px;\n";
    cout << "    background-color: white;\n";
    cout << "    padding: 15px;\n";
    cout << "    border: 1px solid #ccc;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".source-box a {\n";
    cout << "    display: block;\n";
    cout << "    color: #0066cc;\n";
    cout << "    text-decoration: none;\n";
    cout << "    padding: 5px;\n";
    cout << "}\n";
    cout << "\n";
    
    cout << ".source-box a:hover {\n";
    cout << "    text-decoration: underline;\n";
    cout << "}\n";
    
    return 0;
}