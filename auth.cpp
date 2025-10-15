#include <iostream>     
#include <string>       
#include <ctime>        
#include <sstream>     
#include <mysql/mysql.h>  
#include <cstdlib>     
#include <cstring>  

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

static char *spc_escape_sql(const char *input, char quote, int wildcards) {
  char       *out, *ptr;
  const char *c;
   
  if (quote != '\'' && quote != '\"') return 0;
  if (!(out = ptr = (char *)malloc(strlen(input) * 2 + 2 + 1))) return 0;
  *ptr++ = quote;
  for (c = input;  *c;  c++) {
    switch (*c) {
      case '\'': case '\"':
        if (quote == *c) *ptr++ = *c;
        *ptr++ = *c;
        break;
      case '%': case '_': case '[': case ']':
        if (wildcards) *ptr++ = '\\';
        *ptr++ = *c;
        break;
      case '\\': *ptr++ = '\\'; *ptr++ = '\\'; break;
      case '\b': *ptr++ = '\\'; *ptr++ = 'b';  break;
      case '\n': *ptr++ = '\\'; *ptr++ = 'n';  break;
      case '\r': *ptr++ = '\\'; *ptr++ = 'r';  break;
      case '\t': *ptr++ = '\\'; *ptr++ = 't';  break;
      default:
        *ptr++ = *c;
        break;
    }
  }
  *ptr++ = quote;
  *ptr = 0;
  return out;
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
    string confirmPassword = getPostValue(postData, "confirm_password");   
    
  
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
        cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";  
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
    
        if (email.empty() || password.empty() || confirmPassword.empty()) {
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
            cout << "      <div class=\"message error\">All fields are required.</div>\n";  
            cout << "      <a href=\"index.cgi\">Go Back</a>\n";  
            cout << "    </div>\n";  
            cout << "  </body>\n";  
            cout << "</html>\n";   
            mysql_close(conn);   
            return 0;    
        }
        
        // Check if passwords match
        if (password != confirmPassword) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Registration Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
            cout << "  </head>\n";
            cout << "  <body>\n";
            cout << "    <div class=\"container\">\n";
            cout << "      <div class=\"message error\">Passwords do not match.</div>\n";
            cout << "      <a href=\"index.cgi\">Go Back</a>\n";
            cout << "    </div>\n";
            cout << "  </body>\n";
            cout << "</html>\n";
            mysql_close(conn);
            return 0;
        }
        
        // Use spc_escape_sql to build quoted, escaped values for SQL
        char *qEmail = spc_escape_sql(email.c_str(), '\'', 0);
        if (!qEmail) {
            // fallback error page
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n<html><body><p>Server error.</p></body></html>\n";
            mysql_close(conn);
            return 1;
        }

        string checkQuery = string("SELECT user_id FROM users WHERE email=") + qEmail;
        free(qEmail); // free after building query

        // Execute the query
        if (mysql_query(conn, checkQuery.c_str())) { 
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Database Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
        
        // prepare email and password for insert (quoted & escaped)
        char *qEmail2 = spc_escape_sql(email.c_str(), '\'', 0);
        char *qPass2  = spc_escape_sql(password.c_str(), '\'', 0);
        if (!qEmail2 || !qPass2) {
            // cleanup and error
            free(qEmail2); free(qPass2);
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n<html><body><p>Server error.</p></body></html>\n";
            mysql_close(conn);
            return 1;
        }

        string insertQuery = string("INSERT INTO users (email, password) VALUES (") + qEmail2 + ", " + qPass2 + ")";
        free(qEmail2); free(qPass2);

        if (mysql_query(conn, insertQuery.c_str())) {   
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Registration Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
        cout << "Content-type: text/html\r\n\r\n";   
        cout << "<!DOCTYPE html>\n";    
        cout << "<html lang=\"en\">\n";  
        cout << "  <head>\n";       
        cout << "    <meta charset=\"UTF-8\">\n";  
        cout << "    <meta http-equiv=\"refresh\" content=\"2;url=./index.cgi\">\n";  
        cout << "    <title>Registration Successful</title>\n";   
        cout << "    <link rel=\"stylesheet\" href=\"./style.css\">\n";  
        cout << "  </head>\n";       
        cout << "  <body>\n";    
        cout << "    <div class=\"container\">\n";  
        cout << "      <div class=\"message success\">Registration successful! Redirecting...</div>\n";  
        cout << "    </div>\n";       
        cout << "  </body>\n";       
        cout << "</html>\n";           
        
    } else if (action == "login") {
     
        
   
        if (email.empty() || password.empty()) {
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Login Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
        
        // Build safe, quoted strings for email and password
        char *qEmailL = spc_escape_sql(email.c_str(), '\'', 0);
        char *qPassL  = spc_escape_sql(password.c_str(), '\'', 0);
        if (!qEmailL || !qPassL) {
            free(qEmailL); free(qPassL);
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n<html><body><p>Server error.</p></body></html>\n";
            mysql_close(conn);
            return 1;
        }

        string loginQuery = string("SELECT user_id FROM users WHERE email=") + qEmailL + " AND password=" + qPassL;
        free(qEmailL); free(qPassL);

        if (mysql_query(conn, loginQuery.c_str())) {   
            cout << "Content-type: text/html\r\n\r\n";
            cout << "<!DOCTYPE html>\n";
            cout << "<html lang=\"en\">\n";
            cout << "  <head>\n";
            cout << "    <meta charset=\"UTF-8\">\n";
            cout << "    <title>Database Error</title>\n";
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
            cout << "    <link rel=\"stylesheet\" href=\"style.css\">\n";
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
        cout << "Content-type: text/html\r\n\r\n";  
        cout << "<!DOCTYPE html>\n";  
        cout << "<html lang=\"en\">\n";   
        cout << "  <head>\n";   
        cout << "    <meta charset=\"UTF-8\">\n";  
        cout << "    <meta http-equiv=\"refresh\" content=\"2;url=./index.cgi\">\n";  
        cout << "    <title>Login Successful</title>\n";  
                    cout << "    <link rel=\"stylesheet\" href=\"./style.cgi\">\n"; 
        cout << "  </head>\n";   
        cout << "  <body>\n";   
        cout << "    <div class=\"container\">\n";  
        cout << "      <div class=\"message success\">Login successful! Redirecting...</div>\n";  
        cout << "    </div>\n";     
        cout << "  </body>\n";   
        cout << "</html>\n";  
        
    } else if (action == "logout") {
         
        cout << "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly\r\n"; 
        cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\n";  
        cout << "<html lang=\"en\">\n";  
        cout << "  <head>\n";   
        cout << "    <meta charset=\"UTF-8\">\n"; 
        cout << "    <meta http-equiv=\"refresh\" content=\"2;url=./index.cgi\">\n";  
        cout << "    <title>Logged Out</title>\n";   
        cout << "    <link rel=\"stylesheet\" href=\"./style.css\">\n";  
        cout << "  </head>\n";          
        cout << "  <body>\n";           
        cout << "    <div class=\"container\">\n";   
        cout << "      <div class=\"message success\">Logged out successfully!</div>\n";  
        cout << "    </div>\n";      
        cout << "  </body>\n";    
        cout << "</html>\n";               
        
    } else {
        // Invalid action shows error page
        cout << "Content-type: text/html\r\n\r\n";  
        cout << "<!DOCTYPE html>\n";      
        cout << "<html lang=\"en\">\n";   
        cout << "  <head>\n";             
        cout << "    <meta charset=\"UTF-8\">\n";  
        cout << "    <title>Error</title>\n";  
        cout << "    <link rel=\"stylesheet\" href=\"./style.css\">\n";  
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