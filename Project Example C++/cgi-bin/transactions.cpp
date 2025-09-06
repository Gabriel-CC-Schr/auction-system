// transactions.cpp — C++17 CGI: user's transactions dashboard
// SSL/TLS: ignore option files + do NOT require TLS (same as your other CGIs)

#include <bits/stdc++.h>
#include <mariadb/mysql.h>
using namespace std;

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

// ---------- small helpers ----------
static string html_escape(const string& s){
    string o; o.reserve(s.size());
    for(char c : s){
        switch(c){
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'':o += "&#39;"; break;
            default:  o.push_back(c);
        }
    }
    return o;
}

static void header(){ cout << "Content-Type: text/html\r\n\r\n"; }
static void top(){
    cout << "<!doctype html><html><head><meta charset='utf-8'><title>Your Transactions</title>"
         << "<style>body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}"
         << "h2{margin-top:28px}"
         << "table{border-collapse:collapse;width:100%}"
         << "th,td{border:1px solid #ddd;padding:8px}"
         << ".warn{color:#b00;font-weight:bold}</style></head><body>"
         << "<h1>Your Transactions</h1>"
         << "<p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · "
         << "<a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a></p><hr>";
}
static void bottom(){ cout << "</body></html>"; }

// ---------- DB ----------
static MYSQL* db_connect(){
    MYSQL* conn = mysql_init(nullptr);
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // (1) Ignore my.ini/option files entirely (prevents hidden SSL flags)
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_NOCONF)
    { unsigned int nocnf = 1; mysql_options(conn, MARIADB_OPT_NOCONF, &nocnf); }
#endif
    // (2) Do NOT enforce TLS (works even if server has no TLS)
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)
    { unsigned int enforce = 0; mysql_options(conn, MARIADB_OPT_SSL_ENFORCE, &enforce); }
#endif
    // (3) If libmysql is ever used, explicitly disable TLS
#if defined(MYSQL_OPT_SSL_MODE)
#ifndef SSL_MODE_DISABLED
#define SSL_MODE_DISABLED 0
#endif
    { int mode = SSL_MODE_DISABLED; mysql_options(conn, MYSQL_OPT_SSL_MODE, &mode); }
#endif

    if(!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr;
    return conn;
}

static string esc(MYSQL* c, const string& in){
    string out; out.resize(in.size()*2+1);
    unsigned long n = mysql_real_escape_string(c, out.data(), in.c_str(), in.size());
    out.resize(n);
    return out;
}

// ---------- session / auth ----------
static string get_cookie_raw(){
    const char* ck = getenv("HTTP_COOKIE");
    return ck ? string(ck) : string();
}
static string get_cookie_value(const string& name){
    string all = get_cookie_raw();
    if(all.empty()) return "";
    string key = name + "=";
    size_t p = all.find(key);
    if(p == string::npos) return "";
    size_t s = p + key.size();
    size_t e = all.find(';', s);
    return all.substr(s, (e==string::npos? all.size():e) - s);
}
static long current_user(MYSQL* c){
    string token = get_cookie_value("SESSION_ID");
    if(token.empty()) return -1;
    string t = esc(c, token);
    string q = "SELECT user_id FROM sessions "
               "WHERE session_id='"+t+"' AND expires_at>NOW() LIMIT 1";
    if(mysql_query(c, q.c_str())!=0) return -1;
    MYSQL_RES* r = mysql_store_result(c);
    if(!r || mysql_num_rows(r)==0){ if(r) mysql_free_result(r); return -1; }
    MYSQL_ROW row = mysql_fetch_row(r);
    long uid = row && row[0] ? atol(row[0]) : -1;
    mysql_free_result(r);
    return uid;
}

// ---------- table printer ----------
static void run_and_print_table(MYSQL* db,
                                const string& sql,
                                const vector<string>& headers){
    if(mysql_query(db, sql.c_str())!=0){
        cout << "<p class='warn'>Query failed.</p>";
        return;
    }
    MYSQL_RES* res = mysql_store_result(db);
    if(!res){ cout << "<p class='warn'>No results.</p>"; return; }
    if(mysql_num_rows(res)==0){
        cout << "<p><em>None.</em></p>";
        mysql_free_result(res);
        return;
    }
    cout << "<table><thead><tr>";
    for(const auto& h : headers) cout << "<th>" << html_escape(h) << "</th>";
    cout << "</tr></thead><tbody>";
    MYSQL_ROW row;
    unsigned int ncols = mysql_num_fields(res);
    while((row = mysql_fetch_row(res))){
        cout << "<tr>";
        for(unsigned int i=0;i<ncols;i++){
            const char* v = row[i] ? row[i] : "";
            cout << "<td>" << v << "</td>"; // values already come from server (safe in this context)
        }
        cout << "</tr>";
    }
    cout << "</tbody></table>";
    mysql_free_result(res);
}

// ---------- main ----------
int main(){
    header();
    top();

    MYSQL* db = db_connect();
    if(!db){ cout << "<p class='warn'>DB connection failed.</p>"; bottom(); return 0; }

    long uid = current_user(db);
    if(uid < 0){
        cout << "<p class='warn'>You are not logged in. "
             << "<a href='/cgi-bin/auth.cgi'>Log in</a></p>";
        mysql_close(db); bottom(); return 0;
    }

    // 1) SELLING — Active
    cout << "<h2>1. Selling — Active</h2>";
    {
        vector<string> cols = {"Item", "Starting Price", "Ends", "Current Highest"};
        string q =
          "SELECT i.title, "
          "FORMAT(i.starting_price,2), "
          "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
          "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) "
          "FROM items i JOIN auctions a ON a.item_id=i.item_id "
          "WHERE i.seller_id=" + to_string(uid) + " AND a.end_time>NOW() AND a.closed=0 "
          "ORDER BY a.end_time ASC";
        run_and_print_table(db, q, cols);
    }

    // 1b) SELLING — Sold
    cout << "<h2>1. Selling — Sold</h2>";
    {
        vector<string> cols = {"Item", "Ended", "Winning Bid"};
        string q =
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
          "FORMAT(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id), i.starting_price),2) "
          "FROM items i JOIN auctions a ON a.item_id=i.item_id "
          "WHERE i.seller_id=" + to_string(uid) + " AND a.end_time<=NOW() "
          "ORDER BY a.end_time DESC";
        run_and_print_table(db, q, cols);
    }

    // 2) PURCHASES — items you won
    cout << "<h2>2. Purchases</h2>";
    {
        vector<string> cols = {"Item", "Ended", "Your Winning Bid"};
        string q =
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "
          "FROM auctions a "
          "JOIN items i ON i.item_id=a.item_id "
          "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "
          "JOIN bids b ON b.auction_id=a.auction_id AND b.amount=m.max_amt "
          "WHERE a.end_time<=NOW() AND b.bidder_id=" + to_string(uid) + " "
          "ORDER BY a.end_time DESC";
        run_and_print_table(db, q, cols);
    }

    // 3) CURRENT BIDS — with status + action
    cout << "<h2>3. Current Bids</h2>";
    cout << "<p>Click “Increase Max Bid” to raise your bid.</p>";
    {
        vector<string> cols = {"Item", "Ends", "Your Max Bid", "Current Highest", "Status", "Action"};
        string q =
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
          "FORMAT(ub.max_amt,2), "
          "FORMAT(GREATEST(IFNULL(allb.max_all,0), i.starting_price),2), "
          "CASE WHEN ub.max_amt >= GREATEST(IFNULL(allb.max_all,0), i.starting_price) "
          "     AND ub.max_amt = allb.max_all THEN 'Leading' ELSE 'Outbid' END AS status, "
          "CONCAT('<a href=\"/cgi-bin/bid_sell.cgi?mode=bid&auction_id=', a.auction_id, '\">Increase Max Bid</a>') "
          "FROM auctions a "
          "JOIN items i ON i.item_id=a.item_id "
          "JOIN (SELECT auction_id, bidder_id, MAX(amount) AS max_amt FROM bids WHERE bidder_id=" + to_string(uid) + " GROUP BY auction_id, bidder_id) ub "
          "  ON ub.auction_id=a.auction_id "
          "LEFT JOIN (SELECT auction_id, MAX(amount) AS max_all FROM bids GROUP BY auction_id) allb "
          "  ON allb.auction_id=a.auction_id "
          "WHERE a.end_time>NOW() AND a.closed=0 "
          "ORDER BY a.end_time ASC";
        run_and_print_table(db, q, cols);
        cout << "<p class='warn'>If “Status” says Outbid, your max bid is lower than someone else’s.</p>";
    }

    // 4) DIDN'T WIN — items you bid on but lost
    cout << "<h2>4. Didn’t Win</h2>";
    {
        vector<string> cols = {"Item", "Ended", "Winning Bid"};
        string q =
          "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "
          "FROM auctions a "
          "JOIN items i ON i.item_id=a.item_id "
          "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "
          "LEFT JOIN (SELECT auction_id, bidder_id, MAX(amount) AS mymax FROM bids WHERE bidder_id=" + to_string(uid) + " GROUP BY auction_id, bidder_id) me "
          "  ON me.auction_id=a.auction_id "
          "WHERE a.end_time<=NOW() AND (me.mymax IS NULL OR me.mymax < m.max_amt) "
          "ORDER BY a.end_time DESC";
        run_and_print_table(db, q, cols);
    }

    mysql_close(db);
    bottom();
    return 0;
}