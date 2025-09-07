// STEP 4: Back-end Auctions API (C++ CGI) — public GET listings; POST bid/sell; server-only self-bid block

#include "api_common.hpp"

// Return all open auctions ordered by ending soonest (no auth required)
static void list_open(MYSQL* c){
    const char* q =
        "SELECT a.auction_id, i.title, i.description, "
        "FORMAT(i.starting_price,2) AS startp, "
        "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS currentp, "
        "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s') AS ends_at "
        "FROM auctions a JOIN items i ON i.item_id=a.item_id "
        "WHERE a.end_time>NOW() AND a.closed=0 "
        "ORDER BY a.end_time ASC";

    if(mysql_query(c, q) != 0){ cout << "[]"; return; }
    MYSQL_RES* r = mysql_store_result(c); if(!r){ cout << "[]"; return; }

    cout << "[";
    bool first = true;
    MYSQL_ROW row;
    while((row = mysql_fetch_row(r))){
        if(!first) cout << ","; first = false;
        string aid   = row[0]?row[0]:"";
        string title = row[1]?row[1]:"";
        string descr = row[2]?row[2]:"";
        string sp    = row[3]?row[3]:"0.00";
        string cp    = row[4]?row[4]:"0.00";
        string ends  = row[5]?row[5]:"";
        cout << "{"
             << "\"auction_id\":\"" << json_escape(aid)   << "\","
             << "\"title\":\""      << json_escape(title) << "\","
             << "\"description\":\""<< json_escape(descr) << "\","
             << "\"starting\":\""   << json_escape(sp)    << "\","
             << "\"current\":\""    << json_escape(cp)    << "\","
             << "\"ends\":\""       << json_escape(ends)  << "\""
             << "}";
    }
    cout << "]";
    mysql_free_result(r);
}

int main(){
    string method = getenv("REQUEST_METHOD")?getenv("REQUEST_METHOD"):"GET";
    MYSQL* c = db(); if(!c){ http_headers(); cout << R"({"ok":false,"error":"db"})"; return 0; }

    if(method == "GET"){
        // Public: anyone can view current listings ordered by ending soonest
        http_headers();
        cout << "{\"ok\":true,\"auctions\":";
        list_open(c);
        cout << "}";
        mysql_close(c);
        return 0;
    }

    // POST requires a valid (non-expired) session
    long uid = current_user(c);
    if(uid < 0){
        http_headers_with_status(401, "Unauthorized");
        cout << R"({"ok":false,"error":"auth_timeout"})";
        mysql_close(c);
        return 0;
    }

    auto kv = parse_form(read_body());
    const string action = kv["action"];

    if(action == "bid"){
        const string auction_id = kv["auction_id"];
        const string amount     = kv["amount"];
        if(auction_id.empty() || amount.empty()){
            http_headers(); cout << R"({"ok":false,"error":"bad_input"})"; mysql_close(c); return 0;
        }

        // Load auction state: seller, starting price, current high, closed flag
        const char* q =
            "SELECT i.seller_id, i.starting_price, "
            "GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price) AS cur, "
            "a.closed "
            "FROM auctions a "
            "JOIN items i ON i.item_id=a.item_id "
            "WHERE a.auction_id=? LIMIT 1";

        MYSQL_STMT* s = mysql_stmt_init(c);
        mysql_stmt_prepare(s, q, (unsigned long)strlen(q));
        unsigned long aidlen = 0; MYSQL_BIND pb = bind_in_str(auction_id, aidlen);
        mysql_stmt_bind_param(s, &pb);

        if(mysql_stmt_execute(s) != 0){ mysql_stmt_close(s); http_headers(); cout << R"({"ok":false,"error":"auction"})"; mysql_close(c); return 0; }

        long long seller = 0; double starting = 0.0, current = 0.0; int closed = 0;
        MYSQL_BIND rb[4]{}; rb[0].buffer_type=MYSQL_TYPE_LONGLONG; rb[0].buffer=&seller;
        rb[1].buffer_type=MYSQL_TYPE_DOUBLE;   rb[1].buffer=&starting;
        rb[2].buffer_type=MYSQL_TYPE_DOUBLE;   rb[2].buffer=&current;
        rb[3].buffer_type=MYSQL_TYPE_LONG;     rb[3].buffer=&closed;
        mysql_stmt_bind_result(s, rb);

        if(mysql_stmt_store_result(s)!=0 || mysql_stmt_fetch(s)!=0){
            mysql_stmt_free_result(s); mysql_stmt_close(s);
            http_headers(); cout << R"({"ok":false,"error":"auction"})"; mysql_close(c); return 0;
        }
        mysql_stmt_free_result(s); mysql_stmt_close(s);

        // Server-only self-bid protection
        if((long)seller == uid){
            http_headers_with_status(403, "Forbidden");
            cout << R"({"ok":false,"error":"self_bid"})";
            mysql_close(c);
            return 0;
        }
        if(closed){
            http_headers(); cout << R"({"ok":false,"error":"closed"})"; mysql_close(c); return 0;
        }

        double amt = atof(amount.c_str());
        if(amt < starting || amt <= current){
            http_headers(); cout << R"({"ok":false,"error":"low"})"; mysql_close(c); return 0;
        }

        // Insert bid
        const char* qi = "INSERT INTO bids(auction_id,bidder_id,amount) VALUES(?,?,?)";
        MYSQL_STMT* si = mysql_stmt_init(c);
        mysql_stmt_prepare(si, qi, (unsigned long)strlen(qi));
        long long uidll = uid; double amtd = amt;
        MYSQL_BIND ib[3]{}; unsigned long aidl=0;
        ib[0] = bind_in_str(auction_id, aidl);
        ib[1] = bind_in_longlong(uidll);
        ib[2] = bind_in_double(amtd);
        mysql_stmt_bind_param(si, ib);

        if(mysql_stmt_execute(si) != 0){
            mysql_stmt_close(si); http_headers(); cout << R"({"ok":false,"error":"insert"})"; mysql_close(c); return 0;
        }
        mysql_stmt_close(si);

        http_headers(); cout << R"({"ok":true})"; mysql_close(c); return 0;
    }
    else if(action == "sell"){
        const string title = kv["title"];
        const string descr = kv["description"];
        const string sp    = kv["starting_price"];
        const string st    = kv["start_time"]; // "YYYY-MM-DD HH:MM:SS"

        if(title.empty() || descr.empty() || sp.empty() || st.empty()){
            http_headers(); cout << R"({"ok":false,"error":"bad_input"})"; mysql_close(c); return 0;
        }

        // Insert item
        const char* qi = "INSERT INTO items(seller_id,title,description,starting_price) VALUES(?,?,?,?)";
        MYSQL_STMT* si = mysql_stmt_init(c);
        mysql_stmt_prepare(si, qi, (unsigned long)strlen(qi));
        long long uidll = uid; double price = atof(sp.c_str());
        unsigned long tlen=0, dlen=0;
        MYSQL_BIND ib[4]{}; ib[0]=bind_in_longlong(uidll); ib[1]=bind_in_str(title,tlen);
        ib[2]=bind_in_str(descr,dlen); ib[3]=bind_in_double(price);
        mysql_stmt_bind_param(si, ib);

        if(mysql_stmt_execute(si) != 0){
            mysql_stmt_close(si); http_headers(); cout << R"({"ok":false,"error":"item"})"; mysql_close(c); return 0;
        }
        mysql_stmt_close(si);
        my_ulonglong item_id = mysql_insert_id(c);

        // Insert auction with 7-day duration from start_time
        const char* qa = "INSERT INTO auctions(item_id,start_time,end_time) VALUES(?, ?, DATE_ADD(?, INTERVAL 168 HOUR))";
        MYSQL_STMT* sa = mysql_stmt_init(c);
        mysql_stmt_prepare(sa, qa, (unsigned long)strlen(qa));
        long long iid = (long long)item_id; unsigned long st1=0, st2=0;
        MYSQL_BIND ab[3]{}; ab[0]=bind_in_longlong(iid); ab[1]=bind_in_str(st,st1); ab[2]=bind_in_str(st,st2);
        mysql_stmt_bind_param(sa, ab);

        if(mysql_stmt_execute(sa) != 0){
            mysql_stmt_close(sa); http_headers(); cout << R"({"ok":false,"error":"auction"})"; mysql_close(c); return 0;
        }
        mysql_stmt_close(sa);

        http_headers(); cout << R"({"ok":true})"; mysql_close(c); return 0;
    }

    http_headers(); cout << R"({"ok":false,"error":"unknown"})";
    mysql_close(c);
    return 0;
}