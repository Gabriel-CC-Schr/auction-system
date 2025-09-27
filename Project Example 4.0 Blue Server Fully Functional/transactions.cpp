// transactions.cpp — build to public_html/transactions.cgi
// g++ -O2 -std=c++17 transactions.cpp -o transactions.cgi -lmysqlclient
#include <bits/stdc++.h>
#include <mysql/mysql.h>
using namespace std;

const string DB_HOST="localhost", DB_USER="allindragons", DB_PASS="snogardnilla_002", DB_NAME="cs370_section2_allindragons";

string get_cookie(const string& name){ string h=getenv("HTTP_COOKIE")?getenv("HTTP_COOKIE"):""; string k=name+"="; size_t p=h.find(k); if(p==string::npos) return ""; size_t e=h.find(';',p); return h.substr(p+k.size(), (e==string::npos?h.size():e)-p-k.size()); }

struct Db{ MYSQL* c=nullptr; Db(){ c=mysql_init(nullptr); mysql_real_connect(c,DB_HOST.c_str(),DB_USER.c_str(),DB_PASS.c_str(),DB_NAME.c_str(),0,nullptr,0);} ~Db(){ if(c) mysql_close(c);} };

bool get_user_from_session(Db& db, const string& token, unsigned long long& uid){
  if(token.empty()) return false;
  const char* sql="SELECT user_id, TIMESTAMPDIFF(SECOND,last_active,NOW()) AS idle FROM sessions WHERE id=?";
  MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql));
  MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_STRING; b.buffer=(void*)token.c_str(); b.buffer_length=token.size(); mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
  MYSQL_BIND out[2]{}; unsigned long long u; long long idle; out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&u; out[1].buffer_type=MYSQL_TYPE_LONGLONG; out[1].buffer=&idle; mysql_stmt_bind_result(s,out);
  bool ok=false; if(mysql_stmt_fetch(s)==0){ if(idle<=300){ uid=u; ok=true; } } mysql_stmt_close(s);
  if(ok){ const char* up="UPDATE sessions SET last_active=NOW() WHERE id=?"; MYSQL_STMT* us=mysql_stmt_init(db.c); mysql_stmt_prepare(us,up,strlen(up)); MYSQL_BIND bb{}; bb.buffer_type=MYSQL_TYPE_STRING; bb.buffer=(void*)token.c_str(); bb.buffer_length=token.size(); mysql_stmt_bind_param(us,&bb); mysql_stmt_execute(us); mysql_stmt_close(us); }
  return ok;
}

void print_head(){ cout << "Content-Type: text/html\r\n\r\n"; cout << "<link rel=\"stylesheet\" href=\"style.css\">"; }

int main(){ ios::sync_with_stdio(false);
  Db db; unsigned long long uid=0; string token=get_cookie("session");
  if(!get_user_from_session(db, token, uid)){ print_head(); cout << "<div class='notice'>Your session expired (5 minutes idle). <a href='auth.html'>Sign in again</a>.</div>"; return 0; }

  print_head(); cout << "<h2>My Transactions</h2>";

  // Selling
  cout << "<div class='card'><h3>Selling</h3>";
  {
    const char* sql = "SELECT id,title,start_price,end_time,status FROM auctions WHERE seller_id=? ORDER BY status='OPEN' DESC, end_time DESC";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_LONGLONG; b.buffer=&uid; mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
    MYSQL_BIND out[5]{}; unsigned long long aid; char title[121]; unsigned long tlen; double start_price; char endbuf[20]; char status[7]; unsigned long slen, elen;
    out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&aid;
    out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=title; out[1].buffer_length=120; out[1].length=&tlen;
    out[2].buffer_type=MYSQL_TYPE_DOUBLE; out[2].buffer=&start_price;
    out[3].buffer_type=MYSQL_TYPE_STRING; out[3].buffer=endbuf; out[3].buffer_length=20; out[3].length=&elen;
    out[4].buffer_type=MYSQL_TYPE_STRING; out[4].buffer=status; out[4].buffer_length=6; out[4].length=&slen;
    mysql_stmt_bind_result(s,out);
    cout << "<ul class='list'>";
    cout.setf(std::ios::fixed); cout<<setprecision(2);
    while(mysql_stmt_fetch(s)==0){ string t(title,tlen), st(status,slen), et(endbuf,elen);
      cout << "<li><span><strong>"<<t<<"</strong> (ends "<<et<<")</span><span>Start $"<<start_price<<" • "<<st<<"</span></li>"; }
    cout << "</ul>"; mysql_stmt_close(s);
  }
  cout << "</div>";

  // Purchases
  cout << "<div class='card'><h3>Purchases</h3>";
  {
    const char* sql = R"SQL(
      SELECT a.id,a.title,MAX(b.amount) AS price
      FROM auctions a JOIN bids b ON b.auction_id=a.id
      WHERE a.status='CLOSED'
        AND b.bidder_id=?
        AND b.amount = (SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id=a.id)
      GROUP BY a.id,a.title
      ORDER BY a.end_time DESC
    )SQL";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_LONGLONG; b.buffer=&uid; mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
    MYSQL_BIND out[3]{}; unsigned long long aid; char title[121]; unsigned long tlen; double price;
    out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&aid;
    out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=title; out[1].buffer_length=120; out[1].length=&tlen;
    out[2].buffer_type=MYSQL_TYPE_DOUBLE; out[2].buffer=&price;
    mysql_stmt_bind_result(s,out);
    cout << "<ul class='list'>";
    cout.setf(std::ios::fixed); cout<<setprecision(2);
    while(mysql_stmt_fetch(s)==0){ cout << "<li><span><strong>"<<string(title,tlen)<<"</strong></span><span>$"<<price<<"</span></li>"; }
    cout << "</ul>"; mysql_stmt_close(s);
  }
  cout << "</div>";

  // Current Bids
  cout << "<div class='card'><h3>Current Bids</h3>";
  {
    const char* sql = R"SQL(
      SELECT a.id, a.title,
             (SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id=a.id) AS top_bid,
             (SELECT MAX(b3.amount) FROM bids b3 WHERE b3.auction_id=a.id AND b3.bidder_id=?) AS my_bid
      FROM auctions a
      WHERE a.status='OPEN' AND EXISTS (
        SELECT 1 FROM bids bx WHERE bx.auction_id=a.id AND bx.bidder_id=?
      )
      ORDER BY a.end_time ASC
    )SQL";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql));
    MYSQL_BIND b[2]{}; b[0].buffer_type=MYSQL_TYPE_LONGLONG; b[0].buffer=&uid; b[1]=b[0];
    mysql_stmt_bind_param(s,b); mysql_stmt_execute(s);
    MYSQL_BIND out[4]{}; unsigned long long aid; char title[121]; unsigned long tlen; double top,my;
    out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&aid;
    out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=title; out[1].buffer_length=120; out[1].length=&tlen;
    out[2].buffer_type=MYSQL_TYPE_DOUBLE; out[2].buffer=&top;
    out[3].buffer_type=MYSQL_TYPE_DOUBLE; out[3].buffer=&my;
    mysql_stmt_bind_result(s,out);
    cout << "<ul class='list'>";
    cout.setf(std::ios::fixed); cout<<setprecision(2);
    while(mysql_stmt_fetch(s)==0){ bool outbid = my < top; cout << "<li><span><strong>"<<string(title,tlen)<<"</strong>"; if(outbid) cout << " <span class='notice'>Outbid</span>"; cout << "</span><span>$"<<top<<" <a href='trade.html'>Increase bid</a></span></li>"; }
    cout << "</ul>"; mysql_stmt_close(s);
  }
  cout << "</div>";

  // Didn't Win
  cout << "<div class='card'><h3>Didn't Win</h3>";
  {
    const char* sql = R"SQL(
      SELECT a.title,
             (SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id=a.id) AS win
      FROM auctions a
      WHERE a.status='CLOSED' AND EXISTS (
        SELECT 1 FROM bids bx WHERE bx.auction_id=a.id AND bx.bidder_id=?
      )
      AND NOT EXISTS (
        SELECT 1 FROM bids bw WHERE bw.auction_id=a.id AND bw.bidder_id=? AND bw.amount = (
          SELECT MAX(b3.amount) FROM bids b3 WHERE b3.auction_id=a.id)
      )
      ORDER BY a.end_time DESC
    )SQL";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); MYSQL_BIND b[2]{}; b[0].buffer_type=MYSQL_TYPE_LONGLONG; b[0].buffer=&uid; b[1]=b[0]; mysql_stmt_bind_param(s,b); mysql_stmt_execute(s);
    MYSQL_BIND out[2]{}; char title[121]; unsigned long tlen; double win;
    out[0].buffer_type=MYSQL_TYPE_STRING; out[0].buffer=title; out[0].buffer_length=120; out[0].length=&tlen;
    out[1].buffer_type=MYSQL_TYPE_DOUBLE; out[1].buffer=&win;
    mysql_stmt_bind_result(s,out);
    cout << "<ul class='list'>";
    cout.setf(std::ios::fixed); cout<<setprecision(2);
    while(mysql_stmt_fetch(s)==0){ cout << "<li><span><strong>"<<string(title,tlen)<<"</strong></span><span>$"<<win<<"</span></li>"; }
    cout << "</ul>"; mysql_stmt_close(s);
  }
  cout << "</div>";

  return 0;
}
