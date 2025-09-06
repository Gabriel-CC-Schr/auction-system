// auth.cpp — auth + sessions with SSL disabled & option files ignored
#include <bits/stdc++.h>
#include <mariadb/mysql.h>
using namespace std;

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

// ---------- helpers ----------
static string url_decode(const string& s){
    string out; out.reserve(s.size());
    for (size_t i=0;i<s.size();++i){
        if (s[i]=='+') out.push_back(' ');
        else if (s[i]=='%' && i+2<s.size()){
            int v=0; sscanf(s.substr(i+1,2).c_str(), "%x", &v);
            out.push_back(char(v)); i+=2;
        } else out.push_back(s[i]);
    }
    return out;
}
static map<string,string> parse_kv(const string& s){
    map<string,string> m;
    size_t start=0;
    while (start < s.size()){
        size_t eq = s.find('=', start);
        size_t amp= s.find('&', start);
        if (eq==string::npos) break;
        string k = url_decode(s.substr(start, eq-start));
        string v = url_decode(s.substr(eq+1, (amp==string::npos?s.size():amp)-(eq+1)));
        m[k]=v;
        if (amp==string::npos) break;
        start = amp+1;
    }
    return m;
}
static string html_escape(const string& s){
    string o; o.reserve(s.size());
    for(char c : s){
        switch(c){
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'':o += "&#39;"; break;
            default: o.push_back(c);
        }
    }
    return o;
}
static string rand_hex(size_t nbytes){
    static random_device rd; static mt19937_64 gen(rd());
    uniform_int_distribution<int> dist(0,255);
    string s; s.reserve(nbytes*2);
    char buf[3];
    for(size_t i=0;i<nbytes;i++){ sprintf(buf,"%02x", dist(gen)); s+=buf; }
    return s;
}

// ---------- DB ----------
static MYSQL* db_connect(){
    MYSQL* conn = mysql_init(nullptr);
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // (1) Ignore my.ini / option files
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF)
    { unsigned int nocnf = 1; mysql_options(conn, MARIADB_OPT_NOCONF, &nocnf); }
#endif
    // (2) Do NOT enforce TLS
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)
    { unsigned int enforce = 0; mysql_options(conn, MARIADB_OPT_SSL_ENFORCE, &enforce); }
#endif
    // (3) If libmysql is used, explicitly disable TLS
#if defined(MYSQL_OPT_SSL_MODE)
#ifndef SSL_MODE_DISABLED
#define SSL_MODE_DISABLED 0
#endif
    { int mode = SSL_MODE_DISABLED; mysql_options(conn, MYSQL_OPT_SSL_MODE, &mode); }
#endif

    if(!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr;
    return conn;
}
static string sql_escape(MYSQL* c, const string& in){
    string out; out.resize(in.size()*2+1);
    unsigned long len = mysql_real_escape_string(c, out.data(), in.c_str(), in.size());
    out.resize(len);
    return out;
}

static void print_header(const string& extra=""){
    cout << "Content-Type: text/html\r\n";
    if (!extra.empty()) cout << extra;
    cout << "\r\n";
}
static void page_top(const string& title){
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>"
         << html_escape(title)
         << "</title><style>body{font-family:sans-serif;max-width:900px;margin:24px auto;padding:0 12px}form{margin:16px 0}input,button,select,textarea{padding:8px;margin:4px 0}a.button,button{border:1px solid #ccc;border-radius:8px;padding:8px 12px;text-decoration:none;display:inline-block}</style></head><body>";
    cout << "<h1>"<< html_escape(title) <<"</h1>";
}
static void page_bottom(){ cout << "</body></html>"; }

static void show_forms(const string& msg=""){
    if(!msg.empty()) cout << "<p style='color:#b00'><strong>"<<html_escape(msg)<<"</strong></p>";
    cout << "<h2>Log in</h2>";
    cout << "<form method='POST'><input type='hidden' name='action' value='login'>"
            "Email: <input type='email' name='email' required><br>"
            "Password: <input type='password' name='password' required><br>"
            "<button type='submit'>Log in</button></form>";

    cout << "<h2>Register</h2>";
    cout << "<form method='POST'><input type='hidden' name='action' value='register'>"
            "Email: <input type='email' name='email' required><br>"
            "Password: <input type='password' name='password' required><br>"
            "<button type='submit'>Create account</button></form>";
    cout << "<hr><p>After logging in, try: "
            "<a class='button' href='/cgi-bin/listings.cgi'>All open auctions</a> · "
            "<a class='button' href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · "
            "<a class='button' href='/cgi-bin/transactions.cgi'>Your transactions</a></p>";
}

static void set_session_cookie(const string& token){
    cout << "Set-Cookie: SESSION_ID="<< token <<"; Path=/; HttpOnly; SameSite=Lax\r\n";
}

static void handle_post(){
    const char* cl = getenv("CONTENT_LENGTH");
    size_t n = cl ? strtoul(cl, nullptr, 10) : 0;
    string body; body.resize(n);
    if(n) fread(body.data(),1,n,stdin);
    auto kv = parse_kv(body);
    string action = kv.count("action")?kv["action"]:"";
    string email  = kv.count("email")?kv["email"]:"";
    string pass   = kv.count("password")?kv["password"]:"";

    print_header();
    page_top("Auction Portal — Auth");

    if(email.empty() || pass.empty() || (action!="register" && action!="login")){
        show_forms("Please fill in all fields.");
        page_bottom(); return;
    }

    MYSQL* db = db_connect();
    if(!db){ show_forms("DB connection failed."); page_bottom(); return; }

    string em = sql_escape(db, email);
    string pw = sql_escape(db, pass);

    if(action=="register"){
        string q = "SELECT user_id FROM users WHERE email='"+em+"' LIMIT 1";
        if(mysql_query(db, q.c_str())==0){
            MYSQL_RES* r = mysql_store_result(db);
            bool exists = mysql_num_rows(r)>0; mysql_free_result(r);
            if(exists){
                show_forms("Email already registered.");
                mysql_close(db); page_bottom(); return;
            }
        }
        string salt = rand_hex(16);
        string ins = "INSERT INTO users(email,password_salt,password_hash) VALUES('"+em+"','"+salt+"',"
                     "SHA2(CONCAT('"+salt+"','"+pw+"'),256))";
        if(mysql_query(db, ins.c_str())!=0){
            show_forms("Registration failed.");
            mysql_close(db); page_bottom(); return;
        }
        my_ulonglong uid = mysql_insert_id(db);
        string tok = rand_hex(32);
        string sess = "INSERT INTO sessions(session_id,user_id,expires_at)"
                      " VALUES('"+tok+"',"+to_string((unsigned long long)uid)+", DATE_ADD(NOW(), INTERVAL 7 DAY))";
        if(mysql_query(db, sess.c_str())==0){
            set_session_cookie(tok);
            cout << "Content-Location: /cgi-bin/listings.cgi\r\n\r\n";
            cout << "<p>Registered! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>";
        } else {
            show_forms("Could not create session, try login.");
        }
        mysql_close(db); page_bottom(); return;
    }

    if(action=="login"){
        string q = "SELECT user_id FROM users WHERE email='"+em+"' "
                   "AND password_hash = SHA2(CONCAT(password_salt,'"+pw+"'),256) LIMIT 1";
        if(mysql_query(db, q.c_str())!=0){
            show_forms("Login query failed."); mysql_close(db); page_bottom(); return;
        }
        MYSQL_RES* r = mysql_store_result(db);
        if(mysql_num_rows(r)==0){
            mysql_free_result(r);
            show_forms("Invalid credentials.");
            mysql_close(db); page_bottom(); return;
        }
        MYSQL_ROW row = mysql_fetch_row(r);
        unsigned long long uid = strtoull(row[0], nullptr, 10);
        mysql_free_result(r);

        string tok = rand_hex(32);
        string sess = "INSERT INTO sessions(session_id,user_id,expires_at)"
                      " VALUES('"+tok+"',"+to_string(uid)+", DATE_ADD(NOW(), INTERVAL 7 DAY))";
        if(mysql_query(db, sess.c_str())==0){
            set_session_cookie(tok);
            cout << "Content-Location: /cgi-bin/listings.cgi\r\n\r\n";
            cout << "<p>Logged in! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>";
        } else {
            show_forms("Could not start session.");
        }
        mysql_close(db);
        page_bottom(); return;
    }
}

int main(){
    const char* rm = getenv("REQUEST_METHOD");
    if(!rm || string(rm)=="GET"){
        print_header();
        page_top("Auction Portal — Auth");
        show_forms();
        page_bottom();
    } else {
        handle_post();
    }
    return 0;
}