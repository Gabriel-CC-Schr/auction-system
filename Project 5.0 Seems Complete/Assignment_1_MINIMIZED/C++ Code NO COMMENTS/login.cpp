#include <iostream>      
#include <string>        
#include <ctime>         
#include <sstream>       
#include <mysql/mysql.h> 
#include <cstdlib>       
using namespace std;
const string DB_HOST = "localhost";                           
const string DB_USER = "allindragons";                        
const string DB_PASS = "snogardnilla_002";                    
const string DB_NAME = "cs370_section2_allindragons";         
string urlDecode(const string& str) {
    string result = "";                  
    for (size_t i = 0; i < str.length(); i++) {  
        if (str[i] == '+') {             
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {  
            int hexValue;                
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hexValue);  
            result += static_cast<char>(hexValue);  
            i += 2;                      
        } else {
            result += str[i];            
        }
    }
    return result;                       
}
string getPostValue(const string& postData, const string& fieldName) {
    size_t pos = postData.find(fieldName + "=");  
    if (pos == string::npos) {           
        return "";                       
    }
    pos += fieldName.length() + 1;       
    size_t endPos = postData.find("&", pos);  
    string value;                        
    if (endPos == string::npos) {        
        value = postData.substr(pos);    
    } else {
        value = postData.substr(pos, endPos - pos);  
    }
    return urlDecode(value);             
}
string escapeSQL(MYSQL* conn, const string& str) {
    char* escaped = new char[str.length() * 2 + 1];  
    mysql_real_escape_string(conn, escaped, str.c_str(), str.length());  
    string result(escaped);              
    delete[] escaped;                    
    return result;                       
}
string getCookie(const string& cookieName) {
    const char* cookies = getenv("HTTP_COOKIE");  
    if (cookies == NULL) {               
        return "";                       
    }
    string cookieStr(cookies);           
    size_t pos = cookieStr.find(cookieName + "=");  
    if (pos == string::npos) {           
        return "";                       
    }
    pos += cookieName.length() + 1;      
    size_t endPos = cookieStr.find(";", pos);  
    if (endPos == string::npos) {        
        return cookieStr.substr(pos);    
    }
    return cookieStr.substr(pos, endPos - pos);  
}
void setSessionCookie(int userId) {
    time_t now = time(0);                
    time_t expireTime = now + 300;       
    string cookieValue = to_string(userId) + ":" + to_string(expireTime);  
    cout << "Set-Cookie: session=" << cookieValue << "; HttpOnly\r\n";
}
int checkSession() {
    string sessionCookie = getCookie("session");  
    if (sessionCookie.empty()) {         
        return 0;                        
    }
    size_t colonPos = sessionCookie.find(":");  
    if (colonPos == string::npos) {      
        return 0;                        
    }
    int userId = atoi(sessionCookie.substr(0, colonPos).c_str());  
    time_t expireTime = atol(sessionCookie.substr(colonPos + 1).c_str());  
    time_t now = time(0);                
    if (now > expireTime) {              
        return 0;                        
    }
    return userId;                       
}
int main() {
    const char* contentLength = getenv("CONTENT_LENGTH");  
    string postData = "";                
    if (contentLength != NULL) {         
        int length = atoi(contentLength);  
        char* buffer = new char[length + 1];  
        cin.read(buffer, length);        
        buffer[length] = '\0';           
        postData = string(buffer);       
        delete[] buffer;                 
    }
    const char* queryString = getenv("QUERY_STRING");  
    string getData = "";                 
    if (queryString != NULL) {           
        getData = string(queryString);   
    }
    string action = getPostValue(postData, "action");          
    if (action.empty()) {                
        action = getPostValue(getData, "action");              
    }
    string email = getPostValue(postData, "email");            
    string password = getPostValue(postData, "password");      
    MYSQL* conn = mysql_init(NULL);      
    if (conn == NULL) {                  
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
        cout << "      <h1>Database Error</h1>\n";  
        cout << "      <p>Failed to initialize database connection.</p>\n";  
        cout << "      <a href=\"index.cgi\">Go Back</a>\n";  
        cout << "    </div>\n";          
        cout << "  </body>\n";           
        cout << "</html>\n";             
        return 1;                        
    }
    if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), 
                          DB_PASS.c_str(), DB_NAME.c_str(), 0, NULL, 0) == NULL) {
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
        cout << "      <h1>Database Error</h1>\n";  
        cout << "      <p>Failed to connect to database: " << mysql_error(conn) << "</p>\n";  
        cout << "      <a href=\"index.cgi\">Go Back</a>\n";  
        cout << "    </div>\n";          
        cout << "  </body>\n";           
        cout << "</html>\n";             
        mysql_close(conn);               
        return 1;                        
    }
    if (action == "register") {
        if (email.empty() || password.empty()) {
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
            cout << "      <div class=\"message error\">Email and password are required.</div>\n";  
            cout << "      <a href=\"index.cgi\">Go Back</a>\n";  
            cout << "    </div>\n";      
            cout << "  </body>\n";       
            cout << "</html>\n";         
            mysql_close(conn);           
            return 0;                    
        }
        string safeEmail = escapeSQL(conn, email);
        string safePassword = escapeSQL(conn, password);
        string checkQuery = "SELECT user_id FROM users WHERE email='" + safeEmail + "'";
        if (mysql_query(conn, checkQuery.c_str())) {  
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
            mysql_close(conn);
            return 0;
        }
        MYSQL_RES* result = mysql_store_result(conn);  
        if (mysql_num_rows(result) > 0) {  
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
            mysql_free_result(result);   
            mysql_close(conn);
            return 0;
        }
        mysql_free_result(result);       
        string insertQuery = "INSERT INTO users (email, password) VALUES ('" + 
                           safeEmail + "', '" + safePassword + "')";
        if (mysql_query(conn, insertQuery.c_str())) {  
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
            mysql_close(conn);
            return 0;
        }
        int newUserId = mysql_insert_id(conn);
        setSessionCookie(newUserId);
        cout << "Location: ./index.cgi\r\n\r\n";  
    } else if (action == "login") {
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
        string safeEmail = escapeSQL(conn, email);
        string safePassword = escapeSQL(conn, password);
        string loginQuery = "SELECT user_id FROM users WHERE email='" + 
                          safeEmail + "' AND password='" + safePassword + "'";
        if (mysql_query(conn, loginQuery.c_str())) {  
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
            mysql_close(conn);
            return 0;
        }
        MYSQL_RES* result = mysql_store_result(conn);  
        if (mysql_num_rows(result) == 0) {  
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
            mysql_free_result(result);
            mysql_close(conn);
            return 0;
        }
        MYSQL_ROW row = mysql_fetch_row(result);
        int userId = atoi(row[0]);       
        mysql_free_result(result);       
        setSessionCookie(userId);        
        cout << "Location: ./index.cgi\r\n\r\n";  
    } else if (action == "logout") {
        cout << "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly\r\n";  
        cout << "Location: ./index.cgi\r\n\r\n";  
    } else {
        cout << "Content-type: text/html\r\n\r\n";  
        cout << "<!DOCTYPE html>\n";     
        cout << "<html lang=\"en\">\n";  
        cout << "  <head>\n";            
        cout << "    <meta charset=\"UTF-8\">\n";  
        cout << "    <title>Error</title>\n";  
        cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n";  
        cout << "  </head>\n";           
        cout << "  <body>\n";            
        cout << "    <div class=\"container\">\n";  
        cout << "      <div class=\"message error\">Invalid action.</div>\n";  
        cout << "      <a href=\"./index.cgi\">Go Back</a>\n";  
        cout << "    </div>\n";          
        cout << "  </body>\n";           
        cout << "</html>\n";             
    }
    mysql_close(conn);                   
    return 0;                            
}
