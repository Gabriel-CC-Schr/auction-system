// STEP 3: Back-end Transactions API (C++ CGI) — 5-min idle timeout enforced (401 on expiry)

#include "api_common.hpp"

static void dump_prepared_array(MYSQL_STMT* st, const vector<string>& cols){
    if(mysql_stmt_execute(st)!=0){ cout<<"[]"; return; }
    MYSQL_RES* meta=mysql_stmt_result_metadata(st); if(!meta){ cout<<"[]"; return; }
    int ncols=mysql_num_fields(meta);
    vector<MYSQL_BIND> rb(ncols); vector<vector<char>> bufs(ncols); vector<unsigned long> lens(ncols); vector<my_bool> isnull(ncols);
    for(int i=0;i<ncols;i++){ rb[i]=MYSQL_BIND{}; rb[i].is_null=&isnull[i]; rb[i].length=&lens[i];
        rb[i].buffer_type=MYSQL_TYPE_STRING; bufs[i].assign(4096,0); rb[i].buffer=bufs[i].data(); rb[i].buffer_length=(unsigned long)bufs[i].size(); }
    mysql_stmt_bind_result(st, rb.data()); mysql_stmt_store_result(st);
    cout<<"["; bool first=true; while(mysql_stmt_fetch(st)==0){ if(!first) cout<<","; first=false; cout<<"{";
        for(int i=0;i<ncols;i++){ string v=isnull[i]?"" : string(bufs[i].data(),lens[i]);
            cout<<"\""<<cols[i]<<"\":\""<<json_escape(v)<<"\""; if(i+1<ncols) cout<<","; }
        cout<<"}"; } cout<<"]"; mysql_free_result(meta);
}

int main(){
    MYSQL* c=db(); if(!c){ http_headers(); cout<<R"({"ok":false,"error":"db"})"; return 0; }
    long uid=current_user(c);
    if(uid<0){ http_headers_with_status(401,"Unauthorized"); cout<<R"({"ok":false,"error":"auth_timeout"})"; mysql_close(c); return 0; }

    http_headers(); cout<<"{\"ok\":true,";

    // Selling — Active
    {
        const char* q="SELECT i.title, FORMAT(i.starting_price,2), DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
                      "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) "
                      "FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id=? AND a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q));
        long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        cout<<"\"selling_active\":"; dump_prepared_array(st, {"title","starting_price","end_time","current_price"});
        mysql_stmt_free_result(st); mysql_stmt_close(st);
    } cout<<",";

    // Selling — Sold
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
                      "FORMAT(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id), i.starting_price),2) "
                      "FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id=? AND a.end_time<=NOW() ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q));
        long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        cout<<"\"selling_sold\":"; dump_prepared_array(st, {"title","ended","winning_bid"});
        mysql_stmt_free_result(st); mysql_stmt_close(st);
    } cout<<",";

    // Purchases
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "
                      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
                      "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "
                      "JOIN bids b ON b.auction_id=a.auction_id AND b.amount=m.max_amt "
                      "WHERE a.end_time<=NOW() AND b.bidder_id=? ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q));
        long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        cout<<"\"purchases\":"; dump_prepared_array(st, {"title","ended","your_bid"});
        mysql_stmt_free_result(st); mysql_stmt_close(st);
    } cout<<",";

    // Current Bids
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "
                      "FORMAT(ub.max_amt,2), FORMAT(GREATEST(IFNULL(allb.max_all,0), i.starting_price),2), "
                      "CASE WHEN ub.max_amt >= GREATEST(IFNULL(allb.max_all,0), i.starting_price) AND ub.max_amt = allb.max_all THEN 'Leading' ELSE 'Outbid' END, "
                      "a.auction_id "
                      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
                      "JOIN (SELECT auction_id, bidder_id, MAX(amount) AS max_amt FROM bids WHERE bidder_id=? GROUP BY auction_id, bidder_id) ub ON ub.auction_id=a.auction_id "
                      "LEFT JOIN (SELECT auction_id, MAX(amount) AS max_all FROM bids GROUP BY auction_id) allb ON allb.auction_id=a.auction_id "
                      "WHERE a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q));
        long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        cout<<"\"current_bids\":"; dump_prepared_array(st, {"title","ends","your_max","current_high","status","auction_id"});
        mysql_stmt_free_result(st); mysql_stmt_close(st);
    } cout<<",";

    // Didn't Win
    {
        const char* q="SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "
                      "FROM auctions a JOIN items i ON i.item_id=a.item_id "
                      "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "
                      "LEFT JOIN (SELECT auction_id, bidder_id, MAX(amount) AS mymax FROM bids WHERE bidder_id=? GROUP BY auction_id, bidder_id) me ON me.auction_id=a.auction_id "
                      "WHERE a.end_time<=NOW() AND (me.mymax IS NULL OR me.mymax < m.max_amt) ORDER BY a.end_time DESC";
        MYSQL_STMT* st=mysql_stmt_init(c); mysql_stmt_prepare(st,q,(unsigned long)strlen(q));
        long long uidll=uid; MYSQL_BIND pb=bind_in_longlong(uidll); mysql_stmt_bind_param(st,&pb);
        cout<<"\"didnt_win\":"; dump_prepared_array(st, {"title","ended","winning_bid"});
        mysql_stmt_free_result(st); mysql_stmt_close(st);
    }

    cout<<"}";
    mysql_close(c); return 0;
}