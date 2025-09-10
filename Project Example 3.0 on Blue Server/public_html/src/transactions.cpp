
#include "../include/cgi_utils.hpp"

using namespace std;
using namespace mini;

static string table(const string& caption, const vector<array<string,4>>& rows){
  string s="<h2>"+caption+"</h2><table><thead><tr><th>Item</th><th>Status</th><th>Price</th><th>When</th></tr></thead><tbody>";
  if(rows.empty()) s+="<tr><td colspan='4'><em>None</em></td></tr>";
  for(auto& r: rows){ s+="<tr><td>"+r[0]+"</td><td>"+r[1]+"</td><td>"+r[2]+"</td><td>"+r[3]+"</td></tr>"; }
  s+="</tbody></table>"; return s;
}

int main(){
  try{
    CGI c; c.parse(); DB db;
    auto uid = require_session(db, c, true);
    if(!uid){
      hdr_ok(); page("Login required","<p class='warn'>Please <a href='/auth.cgi'>log in</a>. (Session may have expired.)</p>"); return 0;
    }
    int user_id=*uid;

    vector<array<string,4>> selling;
    db.exec("SELECT id,title,end_time FROM auctions WHERE seller_id="+to_string(user_id)+" AND status='open' ORDER BY end_time ASC");
    { MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row; while((row=mysql_fetch_row(r))){
        selling.push_back({ "<a href='/listings.cgi?auction_id="+string(row[0])+"'>"+string(row[1])+"</a>","Open","-", string(row[2]) });
      } mysql_free_result(r); }
    db.exec("SELECT a.id,a.title,t.final_price,t.created_at FROM auctions a JOIN transactions t ON t.auction_id=a.id WHERE a.seller_id="
            +to_string(user_id)+" ORDER BY t.created_at DESC");
    { MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row; while((row=mysql_fetch_row(r))){
        selling.push_back({ "<a href='/listings.cgi?auction_id="+string(row[0])+"'>"+string(row[1])+"</a>","Sold","$"+string(row[2]), string(row[3]) });
      } mysql_free_result(r); }

    vector<array<string,4>> purchases;
    db.exec("SELECT a.id,a.title,t.final_price,t.created_at FROM transactions t JOIN auctions a ON a.id=t.auction_id "
            "WHERE t.buyer_id="+to_string(user_id)+" ORDER BY t.created_at DESC");
    { MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row; while((row=mysql_fetch_row(r))){
        purchases.push_back({ "<a href='/listings.cgi?auction_id="+string(row[0])+"'>"+string(row[1])+"</a>","Won","$"+string(row[2]), string(row[3]) });
      } mysql_free_result(r); }

    vector<array<string,4>> current_bids;
    db.exec("SELECT a.id,a.title, IFNULL(h.highest_bid,a.starting_price) AS cur_price, "
            "CASE WHEN IFNULL(h.highest_bid,0)=(SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id=a.id AND b2.bidder_id="+to_string(user_id)+") "
            "THEN 1 ELSE 0 END AS is_top "
            "FROM auctions a "
            "JOIN bids b ON b.auction_id=a.id "
            "LEFT JOIN auction_highest_bid h ON h.auction_id=a.id "
            "WHERE a.status='open' AND b.bidder_id="+to_string(user_id)+" "
            "GROUP BY a.id "
            "ORDER BY a.end_time ASC");
    { MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row; while((row=mysql_fetch_row(r))){
        bool is_top = atoi(row[3])==1;
        string warn = is_top? "Top bidder" : "<span class='warn'>Outbid</span>";
        string inc = "<a class='btn' href='/trade.cgi?mode=bid&auction_id="+string(row[0])+"'>Increase bid</a>";
        current_bids.push_back({ "<a href='/listings.cgi?auction_id="+string(row[0])+"'>"+string(row[1])+"</a>", warn, "$"+string(row[2]), inc });
      } mysql_free_result(r); }

    vector<array<string,4>> didntwin;
    db.exec("SELECT a.id,a.title,t.final_price,t.created_at "
            "FROM auctions a JOIN bids b ON b.auction_id=a.id "
            "JOIN transactions t ON t.auction_id=a.id "
            "WHERE a.status='closed' AND b.bidder_id="+to_string(user_id)+" AND t.buyer_id<>"+to_string(user_id)+" "
            "GROUP BY a.id ORDER BY a.end_time DESC");
    { MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row; while((row=mysql_fetch_row(r))){
        didntwin.push_back({ "<a href='/listings.cgi?auction_id="+string(row[0])+"'>"+string(row[1])+"</a>","Lost","$"+string(row[2]), string(row[3]) });
      } mysql_free_result(r); }

    hdr_ok();
    page("My Transactions",
      "<h1>My Transactions</h1>"
      + table("1. Selling (Open & Sold)", selling)
      + table("2. Purchases (Won)", purchases)
      + table("3. Current Bids", current_bids)
      + table("4. Didn't Win", didntwin)
      + "<p><a href='/listings.cgi'>Back to Listings</a> · <a href='/trade.cgi'>Bid/Sell</a> · <a href='/auth.cgi?action=logout'>Logout</a></p>"
    );
  } catch(const exception& e){
    cout<<"Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n"<<e.what();
  }
}
