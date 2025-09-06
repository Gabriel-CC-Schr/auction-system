// listings.cpp — open listings with SSL disabled & option files ignored
#include <bits/stdc++.h>
#include <mariadb/mysql.h>
using namespace std;

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

static string html_escape(const string&s){ string o; o.reserve(s.size()); for(char c:s){ switch(c){case'&':o+="&amp;";break;case'<':o+="&lt;";break;case'>':o+="&gt;";break;case'"':o+="&quot;";break;case'\'':o+="&#39;";break;default:o.push_back(c);} } return o; }

static void header(){ cout<<"Content-Type: text/html\r\n\r\n"; }
static void top(){ cout<<"<!doctype html><html><head><meta charset='utf-8'><title>Open Auctions</title><style>body{font-family:sans-serif;max-width:1100px;margin:24px auto;padding:0 12px}.card{border:1px solid #ddd;border-radius:12px;padding:12px;margin:12px 0}.grid{display:grid;grid-template-columns:2fr 1fr 1fr;gap:8px}button,a.button{border:1px solid #ccc;border-radius:10px;padding:8px 12px;display:inline-block;text-decoration:none}small{color:#666}</style></head><body><h1>Open Auctions</h1><p><a href='/cgi-bin/auth.cgi'>Login/Register</a> · <a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>"; }
static void bottom(){ cout<<"</body></html>"; }

static MYSQL* db(){
    MYSQL* c=mysql_init(nullptr);
    mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4");

#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF)
    { unsigned int nocnf = 1; mysql_options(c, MARIADB_OPT_NOCONF, &nocnf); }
#endif
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)
    { unsigned int enforce = 0; mysql_options(c, MARIADB_OPT_SSL_ENFORCE, &enforce); }
#endif
#if defined(MYSQL_OPT_SSL_MODE)
#ifndef SSL_MODE_DISABLED
#define SSL_MODE_DISABLED 0
#endif
    { int mode = SSL_MODE_DISABLED; mysql_options(c, MYSQL_OPT_SSL_MODE, &mode); }
#endif

    if(!mysql_real_connect(c, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr;
    return c;
}
static string esc(MYSQL*c, const string& in){ string out; out.resize(in.size()*2+1); unsigned long n=mysql_real_escape_string(c,out.data(),in.c_str(),in.size()); out.resize(n); return out; }

static long current_user(MYSQL* c){
    const char* ck=getenv("HTTP_COOKIE"); if(!ck) return -1;
    string cookies=ck, token="";
    string key="SESSION_ID=";
    size_t p=cookies.find(key);
    if(p!=string::npos){ size_t s=p+key.size(); size_t e=cookies.find(';',s); token=cookies.substr(s, e==string::npos?string::npos:e-s); }
    if(token.empty()) return -1;
    string t=esc(c,token);
    string q="SELECT user_id FROM sessions WHERE session_id='"+t+"' AND expires_at>NOW() LIMIT 1";
    if(mysql_query(c,q.c_str())!=0) return -1;
    MYSQL_RES* r=mysql_store_result(c);
    if(!r||mysql_num_rows(r)==0){ if(r) mysql_free_result(r); return -1; }
    MYSQL_ROW row=mysql_fetch_row(r);
    long uid = atol(row[0]); mysql_free_result(r); return uid;
}

int main(){
    header(); top();
    MYSQL* c = db();
    if(!c){ cout<<"<p style='color:#b00'>DB connection failed.</p>"; bottom(); return 0; }
    long uid = current_user(c);

    string q =
      "SELECT a.auction_id, i.title, i.description, "
      "FORMAT(i.starting_price,2) AS startp, "
      "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS currentp, "
      "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s') AS ends_at, "
      "i.seller_id "
      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
      "WHERE a.end_time>NOW() AND a.closed=0 "
      "ORDER BY a.end_time ASC";
    if(mysql_query(c,q.c_str())!=0){ cout<<"<p>Query failed.</p>"; mysql_close(c); bottom(); return 0; }
    MYSQL_RES* r = mysql_store_result(c);
    MYSQL_ROW row;
    bool any=false;
    while((row=mysql_fetch_row(r))){
        any=true;
        string aid=row[0]?row[0]:"";
        string title=row[1]?row[1]:"";
        string descr=row[2]?row[2]:"";
        string startp=row[3]?row[3]:"0.00";
        string currentp=row[4]?row[4]:"0.00";
        string ends=row[5]?row[5]:"";
        long seller = row[6]?atol(row[6]):0;

        cout << "<div class='card'><div class='grid'>"
             << "<div><h3>"<< html_escape(title) <<"</h3><p>"<< html_escape(descr) <<"</p>"
             << "<small>Ends: "<< ends <<"</small></div>"
             << "<div><strong>Starting</strong><br>$"<< startp <<"</div>"
             << "<div><strong>Current</strong><br>$"<< currentp <<"</div>"
             << "</div>";
        if(uid>0 && uid != seller){
            cout << "<p><a class='button' href='/cgi-bin/bid_sell.cgi?mode=bid&auction_id="<< aid <<"'>Bid</a></p>";
        } else if(uid==seller){
            cout << "<p><small>You are the seller.</small></p>";
        } else {
            cout << "<p><small><a class='button' href='/cgi-bin/auth.cgi'>Log in to bid</a></small></p>";
        }
        cout << "</div>";
    }
    if(!any) cout << "<p><em>No open auctions right now.</em></p>";
    mysql_free_result(r);
    mysql_close(c);
    bottom();
    return 0;
}