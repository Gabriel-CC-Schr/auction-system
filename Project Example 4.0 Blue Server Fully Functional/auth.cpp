// auth.cpp — CGI auth using MariaDB SHA2-224 hashing (fits CHAR(60), no ALTER, no sudo)
// Build: g++ -O2 -std=c++17 auth.cpp -o auth.cgi -lmariadb -I/usr/include/mysql/mysql
#include <bits/stdc++.h>
#include <mysql/mysql.h>
using namespace std;

const string DB_HOST = "localhost";
const string DB_USER = "allindragons";
const string DB_PASS = "snogardnilla_002";
const string DB_NAME = "cs370_section2_allindragons";

// Site-wide pepper (keep this constant)
const string PEPPER  = "site_pepper_2025_minimal_demo";

string url_decode(const string &s){ string o; o.reserve(s.size());
  for(size_t i=0;i<s.size();++i){ if(s[i]=='+') o+=' ';
    else if(s[i]=='%' && i+2<s.size()){ string h=s.substr(i+1,2); o+=char(strtol(h.c_str(),nullptr,16)); i+=2; }
    else o+=s[i]; } return o; }

map<string,string> parse_post(){
  string len_s = getenv("CONTENT_LENGTH")? getenv("CONTENT_LENGTH"):"0";
  int len = atoi(len_s.c_str()); string body(max(0,len), '\0');
  if(len>0) fread(&body[0],1,len,stdin);
  map<string,string> m; size_t i=0;
  while(i<body.size()){
    size_t eq=body.find('=',i); if(eq==string::npos) break;
    size_t amp=body.find('&',eq);
    string k=url_decode(body.substr(i,eq-i));
    string v=url_decode(body.substr(eq+1,(amp==string::npos? body.size():amp)-eq-1));
    m[k]=v; if(amp==string::npos) break; i=amp+1;
  }
  return m;
}

string rand_hex(size_t n){ static const char* hexd="0123456789abcdef";
  random_device rd; mt19937_64 g(rd()); string s; s.reserve(n);
  for(size_t i=0;i<n;i++) s+=hexd[g()%16]; return s; }

void redirect_with_cookie(const string& url, const string& cookie=""){
  cout << "Status: 303 See Other\r\n";
  if(!cookie.empty()) cout << "Set-Cookie: session="<<cookie<<"; Path=/; HttpOnly; SameSite=Lax\r\n";
  cout << "Location: " << url << "\r\nContent-Type: text/html\r\n\r\n";
  cout << "<html><body>Redirecting… <a href='"<<url<<"'>continue</a></body></html>";
}

struct Db {
  MYSQL* c=nullptr;
  Db(){ c=mysql_init(nullptr); mysql_real_connect(c, DB_HOST.c_str(), DB_USER.c_str(), DB_PASS.c_str(), DB_NAME.c_str(), 0, nullptr, 0); }
  ~Db(){ if(c) mysql_close(c); }
};

bool register_user(Db& db, const string& email, const string& password, unsigned long long& out_id){
  // 56-char hex hash fits CHAR(60)
  const char* sql =
    "INSERT INTO users(email, password_hash) "
    "VALUES(?, SHA2(CONCAT(?, ?), 224))";
  MYSQL_STMT* s = mysql_stmt_init(db.c);
  if(mysql_stmt_prepare(s, sql, strlen(sql))) { mysql_stmt_close(s); return false; }
  MYSQL_BIND b[3]{}; b[0].buffer_type=MYSQL_TYPE_STRING; b[0].buffer=(void*)email.c_str(); b[0].buffer_length=email.size();
  b[1].buffer_type=MYSQL_TYPE_STRING; b[1].buffer=(void*)password.c_str(); b[1].buffer_length=password.size();
  b[2].buffer_type=MYSQL_TYPE_STRING; b[2].buffer=(void*)PEPPER.c_str();  b[2].buffer_length=PEPPER.size();
  mysql_stmt_bind_param(s,b); bool ok = (mysql_stmt_execute(s)==0); mysql_stmt_close(s);
  if(!ok) return false;

  const char* q="SELECT id FROM users WHERE email=?";
  s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,q,strlen(q));
  MYSQL_BIND pb{}; pb.buffer_type=MYSQL_TYPE_STRING; pb.buffer=(void*)email.c_str(); pb.buffer_length=email.size();
  mysql_stmt_bind_param(s,&pb); mysql_stmt_execute(s);
  MYSQL_BIND out{}; out.buffer_type=MYSQL_TYPE_LONGLONG; out.buffer=&out_id; mysql_stmt_bind_result(s,&out);
  ok = (mysql_stmt_fetch(s)==0); mysql_stmt_close(s); return ok;
}

bool login_user(Db& db, const string& email, const string& password, unsigned long long& out_id){
  const char* sql =
    "SELECT id FROM users "
    "WHERE email=? AND password_hash = SHA2(CONCAT(?, ?), 224)";
  MYSQL_STMT* s = mysql_stmt_init(db.c);
  if(mysql_stmt_prepare(s, sql, strlen(sql))) { mysql_stmt_close(s); return false; }
  MYSQL_BIND b[3]{}; b[0].buffer_type=MYSQL_TYPE_STRING; b[0].buffer=(void*)email.c_str(); b[0].buffer_length=email.size();
  b[1].buffer_type=MYSQL_TYPE_STRING; b[1].buffer=(void*)password.c_str(); b[1].buffer_length=password.size();
  b[2].buffer_type=MYSQL_TYPE_STRING; b[2].buffer=(void*)PEPPER.c_str();  b[2].buffer_length=PEPPER.size();
  mysql_stmt_bind_param(s,b); mysql_stmt_execute(s);
  MYSQL_BIND out{}; out.buffer_type=MYSQL_TYPE_LONGLONG; out.buffer=&out_id; mysql_stmt_bind_result(s,&out);
  bool ok = (mysql_stmt_fetch(s)==0); mysql_stmt_close(s); return ok;
}

void upsert_session(Db& db, unsigned long long user_id, const string& token){
  const char* sql="REPLACE INTO sessions(id,user_id,last_active) VALUES(?, ?, NOW())";
  MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql));
  MYSQL_BIND b[2]{}; b[0].buffer_type=MYSQL_TYPE_STRING; b[0].buffer=(void*)token.c_str(); b[0].buffer_length=token.size();
  b[1].buffer_type=MYSQL_TYPE_LONGLONG; b[1].buffer=&user_id;
  mysql_stmt_bind_param(s,b); mysql_stmt_execute(s); mysql_stmt_close(s);
}

int main(){
  ios::sync_with_stdio(false);
  string method = getenv("REQUEST_METHOD")? getenv("REQUEST_METHOD"):"GET";
  if(method!="POST"){
    cout << "Content-Type: text/html\r\n\r\n"
         << "<p>Use the forms on <a href='auth.html'>auth.html</a>.</p>";
    return 0;
  }
  auto f = parse_post();
  string action=f["action"], email=f["email"], password=f["password"];
  Db db;

  if(action=="register"){
    if(email.size()<3 || password.size()<8){
      cout<<"Content-Type: text/html\r\n\r\n<p>Invalid input. <a href='auth.html'>Back</a></p>"; return 0;
    }
    unsigned long long id=0;
    if(!register_user(db, email, password, id)){
      cout<<"Content-Type: text/html\r\n\r\n<p>Registration failed (email may exist). <a href='auth.html'>Back</a></p>"; return 0;
    }
    string token = rand_hex(64); upsert_session(db, id, token);
    redirect_with_cookie("index.html", token);
    return 0;
  }

  if(action=="login"){
    unsigned long long id=0;
    if(!login_user(db, email, password, id)){
      cout<<"Content-Type: text/html\r\n\r\n<p>Login failed. <a href='auth.html'>Try again</a></p>"; return 0;
    }
    string token = rand_hex(64); upsert_session(db, id, token);
    redirect_with_cookie("index.html", token);
    return 0;
  }

  if(action=="logout"){
    cout << "Status: 303 See Other\r\n"
            "Set-Cookie: session=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax\r\n"
            "Location: index.html\r\nContent-Type: text/html\r\n\r\nLogged out.";
    return 0;
  }

  cout<<"Content-Type: text/html\r\n\r\n<p>Unsupported action. <a href='auth.html'>Back</a></p>";
  return 0;
}