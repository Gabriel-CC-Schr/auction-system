
#include "../include/cgi_utils.hpp"
#include "../include/auction_utils.hpp"

using namespace std;
using namespace mini;

int main(){
  try{
    CGI c; c.parse(); DB db;
    close_expired(db);

    auto uid = require_session(db, c, true);
    if(!uid){ hdr_ok(); page("Login required","<p class='warn'>Please <a href='/auth.cgi'>log in</a>.</p>"); return 0; }
    int user_id=*uid;

    string msg;
    if(c.method=="POST"){
      string mode=c.params["mode"];
      if(mode=="bid"){
        int aid=atoi(c.params["auction_id"].c_str());
        double amt=atof(c.params["amount"].c_str());
        db.exec("SELECT seller_id,status,end_time FROM auctions WHERE id="+to_string(aid));
        MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row=mysql_fetch_row(r);
        if(!row){ mysql_free_result(r); msg="<p class='warn'>Invalid auction.</p>"; }
        else {
          int seller=atoi(row[0]); string status=row[1]; mysql_free_result(r);
          if(seller==user_id){ msg="<p class='warn'>You cannot bid on your own item.</p>"; }
          else if(status!="open"){ msg="<p class='warn'>Auction is not open.</p>"; }
          else {
            double cur=current_price(db,aid);
            if(amt <= cur){ msg="<p class='warn'>Bid must exceed current price ($"+to_string(cur)+").</p>"; }
            else {
              string q="INSERT INTO bids(auction_id,bidder_id,amount) VALUES("+to_string(aid)+","+to_string(user_id)+","+to_string(amt)+")";
              db.exec(q);
              msg="<p class='ok'>Bid placed.</p>";
            }
          }
        }
      } else if(mode=="sell"){
        string title=c.params["title"], desc=c.params["description"], sprice=c.params["starting_price"], start=c.params["start_time"];
        if(title.empty()||desc.empty()||sprice.empty()||start.empty()){
          msg="<p class='warn'>All fields are required.</p>";
        } else {
          string q="INSERT INTO auctions(seller_id,title,description,starting_price,start_time,end_time,status) VALUES("
                   +to_string(user_id)+", '"+db.esc(title)+"', '"+db.esc(desc)+"', "+db.esc(sprice)+", '"
                   +db.esc(start)+"', DATE_ADD('"+db.esc(start)+"', INTERVAL 168 HOUR), 'open')";
          db.exec(q);
          msg="<p class='ok'>Auction created.</p>";
        }
      }
    }

    string bid_form = "<h2>Bid on an item</h2>"
      "<form method='post' action='/trade.cgi'>"
      "<input type='hidden' name='mode' value='bid'>"
      "<label>Item "+select_open_auctions(db,user_id)+"</label>"
      "<label>Your highest bid (USD) <input name='amount' type='number' step='0.01' min='0.01' required></label>"
      "<button>Place Bid</button>"
      "</form>";

    string sell_form = "<h2>Sell an item</h2>"
      "<form method='post' action='/trade.cgi'>"
      "<input type='hidden' name='mode' value='sell'>"
      "<label>Title <input name='title' maxlength='255' required></label>"
      "<label>Description <textarea name='description' required></textarea></label>"
      "<label>Starting price (USD) <input name='starting_price' type='number' step='0.01' min='0' required></label>"
      "<label>Start date/time (UTC) <input name='start_time' type='datetime-local' required></label>"
      "<button>Create Auction (7 days)</button>"
      "</form>";

    hdr_ok();
    page("Bid or Sell",
      "<h1>Bid or Sell</h1>"+(msg.empty()?"":msg)+bid_form+sell_form+
      "<p><a href='/listings.cgi'>Listings</a> · <a href='/transactions.cgi'>My Transactions</a> · <a href='/auth.cgi?action=logout'>Logout</a></p>"
    );
  } catch(const exception& e){
    cout<<"Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n"<<e.what();
  }
}
