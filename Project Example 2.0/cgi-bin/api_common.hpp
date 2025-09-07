// STEPS 2–4: Shared header used by Step 2 (Auth API), Step 3 (Transactions API), and Step 4 (Auctions API)

#pragma once
#include <bits/stdc++.h>
#include <mariadb/mysql.h>
using namespace std;

// ---- DB config ----
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

// ---- Session idle timeout (minutes) ----
#define IDLE_TIMEOUT_MINUTES 5

// ---- TLS: CA that signed the MariaDB server cert ----
#define DB_SSL_CA_FILE "C:/xampp/mysql/certs/ca.pem"

// ---- HTTP helpers ----
static void http_headers(const string& ctype="application/json", const string& extra=""){
    cout << "Content-Type: " << ctype << "\r\n";
    if(!extra.empty()) cout << extra;
    cout << "\r\n";
}
static void http_headers_with_status(int code, const string& reason,
                                     const string& ctype="application/json",
                                     const string& extra=""){
    cout << "Status: " << code << " " << reason << "\r\n";
    cout << "Content-Type: " << ctype << "\r\n";
    if(!extra.empty()) cout << extra;
    cout << "\r\n";
}

static string read_body(){
    const char* cl = getenv("CONTENT_LENGTH"); size_t n = cl? strtoul(cl,nullptr,10) : 0;
    string b; b.resize(n); if(n) fread(b.data(),1,n,stdin); return b;
}
static string url_decode(const string& s){
    string o; o.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        if(s[i]=='+') o.push_back(' ');
        else if(s[i]=='%' && i+2<s.size()){ int v=0; sscanf(s.substr(i+1,2).c_str(), "%x", &v); o.push_back(char(v)); i+=2; }
        else o.push_back(s[i]);
    } return o;
}
static map<string,string> parse_form(const string& s){
    map<string,string> m; size_t i=0;
    while(i<s.size()){
        size_t e=s.find('=',i); if(e==string::npos) break;
        size_t a=s.find('&',e+1);
        string k=url_decode(s.substr(i,e-i));
        string v=url_decode(s.substr(e+1,(a==string::npos?s.size():a)-(e+1)));
        m[k]=v; if(a==string::npos) break; i=a+1;
    } return m;
}
static string json_escape(const string& s){
    string o; o.reserve(s.size()+8);
    for(unsigned char c: s){
        switch(c){
            case '\"': o += "\\\""; break; case '\\': o += "\\\\";
            case '\b': o += "\\b";  break; case '\f': o += "\\f";
            case '\n': o += "\\n";  break; case '\r': o += "\\r";
            case '\t': o += "\\t";  break;
            default:
                if(c<0x20){ char buf[7]; sprintf(buf,"\\u%04x", c); o+=buf; }
                else o.push_back(c);
        }
    } return o;
}

// ---- DB connect: TLS REQUIRED + verify server cert ----
static MYSQL* db(){
    MYSQL* c = mysql_init(nullptr); if(!c) return nullptr;
    mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4");
#if defined(MYSQL_OPT_SSL_CA)
    mysql_options(c, MYSQL_OPT_SSL_CA, DB_SSL_CA_FILE);
#endif
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)
    { unsigned int on=1; mysql_options(c, MARIADB_OPT_SSL_ENFORCE, &on); }
#endif
#if defined(MYSQL_OPT_SSL_MODE)
#ifndef SSL_MODE_VERIFY_CA
#define SSL_MODE_VERIFY_CA 3
#endif
    { int mode = SSL_MODE_VERIFY_CA; mysql_options(c, MYSQL_OPT_SSL_MODE, &mode); }
#endif
#if defined(MYSQL_OPT_SSL_VERIFY_SERVER_CERT)
    { my_bool verify=1; mysql_options(c, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify); }
#endif
    if(!mysql_real_connect(c, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr;
    return c;
}

// ---- helpers for prepared statements ----
static string rand_hex(size_t nbytes){
    static random_device rd; static mt19937_64 gen(rd()); uniform_int_distribution<int> d(0,255);
    string s; s.reserve(nbytes*2); char buf[3]; for(size_t i=0;i<nbytes;i++){ sprintf(buf,"%02x", d(gen)); s+=buf; } return s;
}
static MYSQL_BIND bind_in_str(const string& s, unsigned long& len){
    MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_STRING; b.buffer=(void*)s.data(); len=(unsigned long)s.size(); b.length=&len; return b;
}
static MYSQL_BIND bind_in_longlong(long long v){
    MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_LONGLONG; b.buffer=&v; b.is_unsigned=0; return b; // keep v alive
}
static MYSQL_BIND bind_in_double(double v){
    MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_DOUBLE; b.buffer=&v; return b; // keep v alive
}
static long fetch_one_long(MYSQL_STMT* stmt){
    long long out=-1; my_bool isnull=0; MYSQL_BIND rb{}; rb.buffer_type=MYSQL_TYPE_LONGLONG; rb.buffer=&out; rb.is_null=&isnull;
    if(mysql_stmt_bind_result(stmt,&rb)!=0) return -1;
    if(mysql_stmt_store_result(stmt)!=0) return -1;
    if(mysql_stmt_fetch(stmt)!=0 || isnull) return -1;
    return (long)out;
}

// ---- Session: sliding idle timeout ----
// Returns user_id or -1 if no/expired. If valid, bumps expires_at by IDLE_TIMEOUT_MINUTES.
static long current_user(MYSQL* c){
    const char* ck = getenv("HTTP_COOKIE"); if(!ck) return -1;
    string cookies=ck, key="SESSION_ID="; size_t p=cookies.find(key); if(p==string::npos) return -1;
    size_t s=p+key.size(); size_t e=cookies.find(';',s); string token=cookies.substr(s,(e==string::npos?cookies.size():e)-s);

    // Check still valid (not expired)
    const char* q1 = "SELECT user_id FROM sessions WHERE session_id=? AND expires_at>NOW() LIMIT 1";
    MYSQL_STMT* st1=mysql_stmt_init(c); if(!st1) return -1;
    mysql_stmt_prepare(st1,q1,(unsigned long)strlen(q1));
    unsigned long tlen=0; MYSQL_BIND pb=bind_in_str(token,tlen);
    mysql_stmt_bind_param(st1,&pb);
    if(mysql_stmt_execute(st1)!=0){ mysql_stmt_close(st1); return -1; }
    long uid = fetch_one_long(st1);
    mysql_stmt_free_result(st1); mysql_stmt_close(st1);
    if(uid<=0) return -1;

    // Bump expiry (sliding window)
    const char* q2 = "UPDATE sessions SET expires_at=DATE_ADD(NOW(), INTERVAL ? MINUTE) WHERE session_id=?";
    MYSQL_STMT* st2=mysql_stmt_init(c); mysql_stmt_prepare(st2,q2,(unsigned long)strlen(q2));
    long long mins = IDLE_TIMEOUT_MINUTES;
    MYSQL_BIND upb[2]; memset(upb,0,sizeof(upb));
    upb[0]=bind_in_longlong(mins); unsigned long tlen2=0; upb[1]=bind_in_str(token,tlen2);
    mysql_stmt_bind_param(st2, upb);
    mysql_stmt_execute(st2); // best-effort; ignore failure
    mysql_stmt_close(st2);

    return uid;
}