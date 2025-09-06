// bid_sell.cpp — bid & sell with SSL disabled & option files ignored
#include <bits/stdc++.h>
#include <mariadb/mysql.h>
using namespace std;

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

static string html_escape(const string&s){ string o; o.reserve(s.size()); for(char c:s){ switch(c){case'&':o+="&amp;";break;case'<':o+="&lt;";break;case'>':o+="&gt;";break;case'"':o+="&quot;";break;case'\'':o+="&#39;";break;default:o.push_back(c);} } return o; }
static string url_decode(const string& s){ string out; out.reserve(s.size()); for(size_t i=0;i<s.size();++i){ if(s[i]=='+') out.push_back(' '); else if(s[i]=='%'&&i+2<s.size()){int v=0; sscanf(s.substr(i+1,2).c_str(), "%x",&v); out.push_back(char(v)); i+=2;} else out.push_back(s[i]); } return out; }
static map<string,string> parse_kv(const string& s){ map<string,string> m; size_t i=0; while(i<s.size()){ size_t e=s.find('=',i); if(e==string::npos) break; size_t a=s.find('&',e+1); string k=url_decode(s.substr(i,e-i)); string v=url_decode(s.substr(e+1,(a==string::npos?s.size():a)-(e+1))); m[k]=v; if(a==string::npos) break; i=a+1; } return m; }

static void header(){ cout<<"Content-Type: text/html\r\n\r\n"; }
static void top(){ cout<<"<!doctype html><html><head><meta charset='utf-8'><title>Bid / Sell</title><style>body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}form{margin:18px 0;padding:12px;border:1px solid #ddd;border-radius:10px}input,select,button,textarea{padding:8px;margin:6px 0}label{display:block;margin-top:8px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ddd;padding:8px}</style></head><body><h1>Bid / Sell</h1><p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>"; }
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

static void show_forms(MYSQL* c, long uid, const string& preselectAuctionId=""){
    cout << "<h2>Bid on an Item</h2>";
    cout << "<form method='POST'><input type='hidden' name='action' value='bid'>";
    cout << "<label>Item:</label><select name='auction_id' required>";
    string q =
      "SELECT a.auction_id, i.title, "
      "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS cur "
      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
      "WHERE a.end_time>NOW() AND a.closed=0 AND i.seller_id<>"+to_string(uid)+" "
      "ORDER BY a.end_time ASC";
    if(mysql_query(c,q.c_str())==0){
        MYSQL_RES* r=mysql_store_result(c); MYSQL_ROW row;
        while((row=mysql_fetch_row(r))){
            string aid=row[0]?row[0]:"", title=row[1]?row[1]:"", cur=row[2]?row[2]:"0.00";
            cout << "<option value='"<< aid <<"' "<<(preselectAuctionId==aid?"selected":"")<<">"
                 << html_escape(title) <<" — Current: $" << cur << "</option>";
        }
        mysql_free_result(r);
    }
    cout << "</select>";
    cout << "<label>Your Maximum Bid (USD):</label><input type='number' name='amount' min='0' step='0.01' required>";
    cout << "<button type='submit'>Place Bid</button></form>";

    cout << "<h2>Sell an Item</h2>";
    cout << "<form method='POST'><input type='hidden' name='action' value='sell'>"
            "<label>Title</label><input type='text' name='title' maxlength='150' required>"
            "<label>Description</label><textarea name='description' rows='4' required></textarea>"
            "<label>Starting Price (USD)</label><input type='number' name='starting_price' min='0' step='0.01' required>"
            "<label>Start Date & Time (YYYY-MM-DD HH:MM:SS)</label><input type='text' name='start_time' placeholder='2025-09-04 12:00:00' required>"
            "<p><em>All auctions last exactly 168 hours (7 days).</em></p>"
            "<button type='submit'>Create Auction</button></form>";
}

int main(){
    header(); top();
    MYSQL* c = db();
    if(!c){ cout<<"<p style='color:#b00'>DB connection failed.</p>"; bottom(); return 0; }
    long uid = current_user(c);
    if(uid<0){ cout<<"<p style='color:#b00'>You are not logged in. <a href='/cgi-bin/auth.cgi'>Log in</a></p>"; mysql_close(c); bottom(); return 0; }

    string method = getenv("REQUEST_METHOD")?getenv("REQUEST_METHOD"):"GET";
    if(method=="GET"){
        string qs = getenv("QUERY_STRING")?getenv("QUERY_STRING"):"";
        auto kv = parse_kv(qs);
        string pre = kv.count("auction_id")?kv["auction_id"]:"";
        show_forms(c, uid, pre);
        mysql_close(c); bottom(); return 0;
    }

    const char* cl = getenv("CONTENT_LENGTH"); size_t n = cl?strtoul(cl,nullptr,10):0;
    string body; body.resize(n); if(n) fread(body.data(),1,n,stdin);
    auto kv = parse_kv(body);
    string action = kv.count("action")?kv["action"]:"";
    if(action=="bid"){
        string auction_id = kv["auction_id"];
        string amount = kv["amount"];
        if(auction_id.empty()||amount.empty()){ cout<<"<p>Missing fields.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0; }
        string aid = esc(c,auction_id);
        string q = "SELECT a.auction_id, i.item_id, i.seller_id, i.starting_price, "
                   "GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price) AS cur, "
                   "a.end_time, a.closed "
                   "FROM auctions a JOIN items i ON i.item_id=a.item_id "
                   "WHERE a.auction_id="+aid+" LIMIT 1";
        if(mysql_query(c,q.c_str())!=0){ cout<<"<p>Bad auction.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0; }
        MYSQL_RES* r=mysql_store_result(c);
        if(!r || mysql_num_rows(r)==0){ cout<<"<p>Invalid auction.</p>"; if(r) mysql_free_result(r); show_forms(c,uid); mysql_close(c); bottom(); return 0; }
        MYSQL_ROW row=mysql_fetch_row(r);
        long seller = atol(row[2]);
        double starting = atof(row[3]);
        double current = atof(row[4]);
        int closed = atoi(row[6]);
        mysql_free_result(r);

        if(seller==uid){ cout<<"<p style='color:#b00'>You cannot bid on your own item.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0; }
        if(closed){ cout<<"<p style='color:#b00'>Auction is closed.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0; }

        double amt = atof(amount.c_str());
        if(amt < starting || amt <= current){
            cout<<"<p style='color:#b00'>Your max bid must be ≥ starting price ($"<<starting<<") and > current highest ($"<<current<<").</p>";
            show_forms(c,uid, auction_id);
            mysql_close(c); bottom(); return 0;
        }

        string ins = "INSERT INTO bids(auction_id,bidder_id,amount) VALUES("+aid+","+to_string(uid)+","+to_string(amt)+")";
        if(mysql_query(c,ins.c_str())!=0){
            cout<<"<p style='color:#b00'>Failed to place bid.</p>";
        } else {
            cout<<"<p>Bid placed successfully.</p>";
        }
        show_forms(c,uid, auction_id);
        mysql_close(c); bottom(); return 0;
    }
    else if(action=="sell"){
        string title = kv["title"], description = kv["description"], start_price = kv["starting_price"], start_time = kv["start_time"];
        if(title.empty()||description.empty()||start_price.empty()||start_time.empty()){
            cout<<"<p>Missing fields.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0;
        }
        string t=esc(c,title), d=esc(c,description), sp=esc(c,start_price), st=esc(c,start_time);
        string insItem = "INSERT INTO items(seller_id,title,description,starting_price) "
                         "VALUES("+to_string(uid)+",'"+t+"','"+d+"',"+sp+")";
        if(mysql_query(c,insItem.c_str())!=0){ cout<<"<p style='color:#b00'>Failed to create item.</p>"; show_forms(c,uid); mysql_close(c); bottom(); return 0; }
        unsigned long long item_id = mysql_insert_id(c);
        string insA = "INSERT INTO auctions(item_id,start_time,end_time) "
                      "VALUES("+to_string(item_id)+", '"+st+"', DATE_ADD('"+st+"', INTERVAL 168 HOUR))";
        if(mysql_query(c,insA.c_str())!=0){
            cout<<"<p style='color:#b00'>Failed to create auction (check datetime format).</p>";
        } else {
            cout<<"<p>Auction created! It will end 7 days after <strong>"<< html_escape(start_time) <<"</strong>.</p>";
        }
        show_forms(c,uid);
        mysql_close(c); bottom(); return 0;
    }
    else{
        cout<<"<p>Unknown action.</p>";
        show_forms(c,uid);
        mysql_close(c); bottom(); return 0;
    }
}