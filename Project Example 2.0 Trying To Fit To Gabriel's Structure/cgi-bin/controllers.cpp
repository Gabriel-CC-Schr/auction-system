#include "controllers.h"
#include "utils.h"
#include "auth.h"
#include "db.h"
#include <cstdlib>
#include <cstring>
#include <vector>

static std::map<std::string,std::string> parse_query(){ return parse_form(getenv_str("QUERY_STRING")); }

// ---------- AUTH ----------
void handle_auth(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body){
    UNUSED(qs);
    if(m==HttpMethod::GET){
        long uid = current_user_and_slide(c);
        http_headers();
        std::cout << "{\"ok\":true,\"user_id\":" << uid << ",\"idle_minutes\":" << IDLE_TIMEOUT_MINUTES << "}";
        return;
    }
    if(m==HttpMethod::POST){
        auto kv = parse_form(body);
        std::string action = kv["action"], email = kv["email"], pass = kv["password"];
        if(email.empty()||pass.empty()||(action!="register"&&action!="login")){
            http_headers(); std::cout << "{\"ok\":false,\"error\":\"bad_input\"}"; return;
        }
        long long uid=0; std::string tok;
        bool ok = (action=="register") ? register_user(c,email,pass,uid,tok)
                                        : login_user(c,email,pass,uid,tok);
        if(!ok){ http_headers(); std::cout << "{\"ok\":false,\"error\":\"invalid_or_exists\"}"; return; }
        http_headers("application/json", make_set_cookie_header(tok));
        std::cout << "{\"ok\":true,\"user_id\":" << uid << "}";
        return;
    }
    http_headers(); std::cout << "{\"ok\":false,\"error\":\"method\"}";
}

// ---------- AUCTIONS ----------
static void list_open(MYSQL* c){
    const char* q =
        "SELECT a.auction_id, i.title, i.description, "
        "FORMAT(i.starting_price,2), "
        "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2), "
        "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s') "
        "FROM auctions a JOIN items i ON i.item_id=a.item_id "
        "WHERE a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
    if(mysql_query(c,q)!=0){ std::cout << "[]"; return; }
    MYSQL_RES* r=mysql_store_result(c); if(!r){ std::cout << "[]"; return; }
    std::cout << "["; bool first=true; MYSQL_ROW row; while((row=mysql_fetch_row(r))){ if(!first) std::cout << ","; first=false;
        std::string aid=row[0]?row[0]:"", title=row[1]?row[1]:"", descr=row[2]?row[2]:"";
        std::string sp=row[3]?row[3]:"0.00", cp=row[4]?row[4]:"0.00", ends=row[5]?row[5]:"";
        std::cout << "{\"auction_id\":\""<<json_escape(aid)<<"\",\"title\":\""<<json_escape(title)
                  <<"\",\"description\":\""<<json_escape(descr)<<"\",\"starting\":\""<<json_escape(sp)
                  <<"\",\"current\":\""<<json_escape(cp)<<"\",\"ends\":\""<<json_escape(ends)<<"\"}"; }
    std::cout << "]"; mysql_free_result(r);
}

void handle_auctions(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body){
    UNUSED(qs);
    if(m==HttpMethod::GET){
        http_headers(); std::cout << "{\"ok\":true,\"auctions\":"; list_open(c); std::cout << "}"; return;
    }
    if(m==HttpMethod::POST){
        long uid = current_user_and_slide(c); if(uid<0){ http_headers_with_status(401,"Unauthorized"); std::cout << "{\"ok\":false,\"error\":\"auth_timeout\"}"; return; }
        auto kv=parse_form(body); std::string action=kv["action"];
        if(action=="bid"){
            std::string auction_id=kv["auction_id"], amount=kv["amount"]; if(auction_id.empty()||amount.empty()){ http_headers(); std::cout << "{\"ok\":false,\"error\":\"bad_input\"}"; return; }
            const char* q="SELECT i.seller_id, i.starting_price, GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price), a.closed FROM auctions a JOIN items i ON i.item_id=a.item_id WHERE a.auction_id=? LIMIT 1";
            MYSQL_STMT* s=mysql_stmt_init(c); mysql_stmt_prepare(s,q,(unsigned long)strlen(q)); unsigned long aidl=0; MYSQL_BIND pb=bind_in_str(auction_id,aidl);
            mysql_stmt_bind_param(s,&pb); if(mysql_stmt_execute(s)!=0){ mysql_stmt_close(s); http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"auction\\\"}"; return; }
            long long seller=0; double starting=0, current=0; int closed=0; MYSQL_BIND rb[4]{}; rb[0].buffer_type=MYSQL_TYPE_LONGLONG; rb[0].buffer=&seller; rb[1].buffer_type=MYSQL_TYPE_DOUBLE; rb[1].buffer=&starting; rb[2].buffer_type=MYSQL_TYPE_DOUBLE; rb[2].buffer=&current; rb[3].buffer_type=MYSQL_TYPE_LONG; rb[3].buffer=&closed; mysql_stmt_bind_result(s,rb);
            if(mysql_stmt_store_result(s)!=0||mysql_stmt_fetch(s)!=0){ mysql_stmt_free_result(s); mysql_stmt_close(s); http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"auction\\\"}"; return; }
            mysql_stmt_free_result(s); mysql_stmt_close(s);
            if((long)seller==uid){ http_headers_with_status(403,"Forbidden"); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"self_bid\\\"}"; return; }
            if(closed){ http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"closed\\\"}"; return; }
            double amt = std::atof(amount.c_str()); if(amt<starting || amt<=current){ http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"low\\\"}"; return; }
            const char* qi="INSERT INTO bids(auction_id,bidder_id,amount) VALUES(?,?,?)"; MYSQL_STMT* si=mysql_stmt_init(c); mysql_stmt_prepare(si,qi,(unsigned long)strlen(qi)); long long uidll=uid; double amtd=amt; MYSQL_BIND ib[3]{}; ib[0]=bind_in_str(auction_id,aidl); ib[1]=bind_in_longlong(uidll); ib[2]=bind_in_double(amtd); mysql_stmt_bind_param(si,ib);
            if(mysql_stmt_execute(si)!=0){ mysql_stmt_close(si); http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"insert\\\"}"; return; } mysql_stmt_close(si);
            http_headers(); std::cout << "{\"ok\":true}"; return;
        } else if(action=="sell"){
            std::string title=kv["title"], descr=kv["description"], sp=kv["starting_price"], st=kv["start_time"]; if(title.empty()||descr.empty()||sp.empty()||st.empty()){ http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"bad_input\\\"}"; return; }
            const char* qi="INSERT INTO items(seller_id,title,description,starting_price) VALUES(?,?,?,?)"; MYSQL_STMT* si=mysql_stmt_init(c); mysql_stmt_prepare(si,qi,(unsigned long)strlen(qi)); long long uidll=uid; double price=std::atof(sp.c_str()); unsigned long tlen=0,dlen=0; MYSQL_BIND ib[4]{}; ib[0]=bind_in_longlong(uidll); ib[1]=bind_in_str(title,tlen); ib[2]=bind_in_str(descr,dlen); ib[3]=bind_in_double(price); mysql_stmt_bind_param(si,ib); if(mysql_stmt_execute(si)!=0){ mysql_stmt_close(si); http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"item\\\"}"; return; } mysql_stmt_close(si); long long item_id=(long long)mysql_insert_id(c);
            const char* qa="INSERT INTO auctions(item_id,start_time,end_time) VALUES(?, ?, DATE_ADD(?, INTERVAL 168 HOUR))"; MYSQL_STMT* sa=mysql_stmt_init(c); mysql_stmt_prepare(sa,qa,(unsigned long)strlen(qa)); long long iid=item_id; unsigned long st1=0,st2=0; MYSQL_BIND ab[3]{}; ab[0]=bind_in_longlong(iid); ab[1]=bind_in_str(st,st1); ab[2]=bind_in_str(st,st2); mysql_stmt_bind_param(sa,ab); if(mysql_stmt_execute(sa)!=0){ mysql_stmt_close(sa); http_headers(); std::cout<<"{\\\"ok\\\":false,\\\"error\\\":\\\"auction\\\"}"; return; } mysql_stmt_close(sa);
            http_headers(); std::cout << "{\"ok\":true}"; return;
        }
        http_headers(); std::cout << "{\"ok\":false,\"error\":\"unknown\"}"; return;
    }
    http_headers(); std::cout << "{\"ok\":false,\"error\":\"method\"}";
}

// ---------- TRANSACTIONS ----------
static void dump_prepared_array(MYSQL_STMT* st, const std::vector<std::string>& cols){
    if(mysql_stmt_execute(st)!=0){ std::cout<<"[]"; return; }
    MYSQL_RES* meta=mysql_stmt_result_metadata(st); if(!meta){ std::cout<<"[]"; return; }
    int ncols=mysql_num_fields(meta);
    std::vector<MYSQL_BIND> rb(ncols); std::vector<std::vector<char>> bufs(ncols); std::vector<unsigned long> lens(ncols); std::vector<my_bool> isnull(ncols);
    for(int i=0;i<ncols;i++){ rb[i]=MYSQL_BIND{}; rb[i].is_null=&isnull[i]; rb[i].length=&lens[i]; rb[i].buffer_type=MYSQL_TYPE_STRING; bufs[i].assign(4096,0); rb[i].buffer=bufs[i].data(); rb[i].buffer_length=(unsigned long)bufs[i].size(); }
    mysql_stmt_bind_result(st, rb.data()); mysql_stmt_store_result(st);
    std::cout<<"["; bool first=true; while(mysql_stmt_fetch(st)==0){ if(!first) std::cout<<","; first=false; std::cout<<"{";
        for(int i=0;i<ncols;i++){ std::string v = isnull[i]? "" : std::string(bufs[i].data(), lens[i]); std::cout<<"\""<<cols[i]<<"\":\""<<json_escape(v)<<"\""; if(i+1<ncols) std::cout<<","; }
        std::cout<<"}"; }
    std::cout<<"]"; mysql_free_result(meta);
}

void handle_transactions(MYSQL* c, HttpMethod m, const std::map<std::string,std::string>& qs, const std::string& body){
    UNUSED(qs); UNUSED(body);
    if(m!=HttpMethod::GET){ http_headers(); std::cout<<"{\"ok\":false,\"error\":\"method\"}"; return; }
    long uid = current_user_and_slide(c); if(uid<0){ http_headers_with_status(401,"Unauthorized"); std::cout<<"{\"ok\":false,\"error\":\"auth_timeout\"}"; return; }

    http_headers(); std::cout<<"{\"ok\":true,";

    // Selling active
    {
        const char* q="SELECT i.title, FORMAT(i.starting_price,2), DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id=? AND a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q)); long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        std::cout<<"\"selling_active\":"; dump_prepared_array(st,{"title","starting_price","end_time","current_price"}); mysql_stmt_free_result(st); mysql_stmt_close(st);
    }
    std::cout<<",";

    // Selling sold
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id), i.starting_price),2) FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id=? AND a.end_time<=NOW() ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q)); long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        std::cout<<"\"selling_sold\":"; dump_prepared_array(st,{"title","ended","winning_bid"}); mysql_stmt_free_result(st); mysql_stmt_close(st);
    }
    std::cout<<",";

    // Purchases
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) FROM auctions a JOIN items i ON i.item_id=a.item_id JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id JOIN bids b ON b.auction_id=a.auction_id AND b.amount=m.max_amt WHERE a.end_time<=NOW() AND b.bidder_id=? ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q)); long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        std::cout<<"\"purchases\":"; dump_prepared_array(st,{"title","ended","your_bid"}); mysql_stmt_free_result(st); mysql_stmt_close(st);
    }
    std::cout<<",";

    // Current bids
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(ub.max_amt,2), FORMAT(GREATEST(IFNULL(allb.max_all,0), i.starting_price),2), CASE WHEN ub.max_amt >= GREATEST(IFNULL(allb.max_all,0), i.starting_price) AND ub.max_amt = allb.max_all THEN 'Leading' ELSE 'Outbid' END, a.auction_id FROM auctions a JOIN items i ON i.item_id=a.item_id JOIN (SELECT auction_id, bidder_id, MAX(amount) AS max_amt FROM bids WHERE bidder_id=? GROUP BY auction_id, bidder_id) ub ON ub.auction_id=a.auction_id LEFT JOIN (SELECT auction_id, MAX(amount) AS max_all FROM bids GROUP BY auction_id) allb ON allb.auction_id=a.auction_id WHERE a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q)); long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        std::cout<<"\"current_bids\":"; dump_prepared_array(st,{"title","ends","your_max","current_high","status","auction_id"}); mysql_stmt_free_result(st); mysql_stmt_close(st);
    }
    std::cout<<",";

    // Didn't win
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) FROM auctions a JOIN items i ON i.item_id=a.item_id JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id LEFT JOIN (SELECT auction_id, bidder_id, MAX(amount) AS mymax FROM bids WHERE bidder_id=? GROUP BY auction_id, bidder_id) me ON me.auction_id=a.auction_id WHERE a.end_time<=NOW() AND (me.mymax IS NULL OR me.mymax < m.max_amt) ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q)); long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        std::cout<<"\"didnt_win\":"; dump_prepared_array(st,{"title","ended","winning_bid"}); mysql_stmt_free_result(st); mysql_stmt_close(st);
    }

    std::cout<<"}";
}
