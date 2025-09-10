
#include "../include/cgi_utils.hpp"
#include "../include/auction_utils.hpp"

using namespace std;
using namespace mini;

static string card(const string& title, const string& desc, const string& price, const string& end, const string& controls){
  return "<div class='card'><h3>"+title+"</h3><p>"+desc+"</p><p><strong>Current:</strong> "+price+
         " &nbsp; <strong>Ends:</strong> "+end+"</p>"+controls+"</div>";
}

int main(){
  try{
    CGI c; c.parse(); DB db;
    close_expired(db);
    auto uid = require_session(db, c, true); int user_id = uid? *uid : -1;

    if(c.params.count("auction_id")){
      int aid = atoi(c.params["auction_id"].c_str());
      db.exec("SELECT a.id,a.title,a.description,a.starting_price,a.end_time,a.seller_id,a.status "
              "FROM auctions a WHERE a.id="+to_string(aid));
      MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row=mysql_fetch_row(r);
      if(!row){ mysql_free_result(r); hdr_ok(); page("Not found","<p>Listing not found.</p>"); return 0; }
      int seller=atoi(row[5]); string status=row[6]; double cur=current_price(db,aid);
      string controls;
      if(uid && status=="open" && seller!=user_id)
        controls = "<p><a class='btn' href='/trade.cgi?mode=bid&auction_id="+to_string(aid)+"'>Bid</a></p>";
      hdr_ok();
      page(row[1],
        string("<h1>")+string(row[1])+"</h1><p>"+string(row[2])+"</p>"
        "<p><strong>Current:</strong> $"+to_string(cur)+" &nbsp; <strong>Ends:</strong> "+string(row[4])+"</p>"
        +controls+"<p><a href='/listings.cgi'>Back</a></p>"
      );
      mysql_free_result(r); return 0;
    }

    db.exec("SELECT a.id,a.title,a.description,a.starting_price,a.end_time,a.seller_id "
            "FROM auctions a WHERE a.status='open' AND a.end_time>UTC_TIMESTAMP() ORDER BY a.end_time ASC");
    MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row;
    string body="<h1>Auctions (ending soonest)</h1>";
    bool any=false;
    while((row=mysql_fetch_row(r))){
      any=true;
      int aid=atoi(row[0]); int seller=atoi(row[5]); double cur=current_price(db,aid);
      string controls;
      if(uid && seller!=user_id)
        controls = "<p><a class='btn' href='/trade.cgi?mode=bid&auction_id="+to_string(aid)+"'>Bid</a></p>";
      body += card("<a href='/listings.cgi?auction_id="+to_string(aid)+"'>"+string(row[1])+"</a>",
                   row[2], "$"+to_string(cur), row[4], controls);
    }
    mysql_free_result(r);
    if(!any) body += "<p><em>No open auctions right now.</em></p>";
    body += "<p><a href='/trade.cgi'>Bid/Sell</a> · <a href='/transactions.cgi'>My Transactions</a> · "
            + string(uid? "<a href='/auth.cgi?action=logout'>Logout</a>" : "<a href='/auth.cgi'>Login</a>") + "</p>";
    hdr_ok(); page("Listings", body);
  } catch(const exception& e){
    cout<<"Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n"<<e.what();
  }
}
