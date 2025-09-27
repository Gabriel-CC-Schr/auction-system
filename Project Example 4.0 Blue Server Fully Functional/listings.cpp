// listings.cpp — build to public_html/listings.cgi
// g++ -O2 -std=c++17 listings.cpp -o listings.cgi -lmysqlclient
#include <bits/stdc++.h>
#include <mysql/mysql.h>
using namespace std;

const string DB_HOST="localhost", DB_USER="allindragons", DB_PASS="snogardnilla_002", DB_NAME="cs370_section2_allindragons";

string get_cookie(const string& name){ string h=getenv("HTTP_COOKIE")?getenv("HTTP_COOKIE"):""; string k=name+"="; size_t p=h.find(k); if(p==string::npos) return ""; size_t e=h.find(';',p); return h.substr(p+k.size(), (e==string::npos?h.size():e)-p-k.size()); }

struct Db{ MYSQL* c=nullptr; Db(){ c=mysql_init(nullptr); mysql_real_connect(c,DB_HOST.c_str(),DB_USER.c_str(),DB_PASS.c_str(),DB_NAME.c_str(),0,nullptr,0);} ~Db(){ if(c) mysql_close(c);} };

unsigned long long current_user(Db& db){
  string token=get_cookie("session"); if(token.empty()) return 0ULL;
  const char* sql="SELECT user_id, TIMESTAMPDIFF(SECOND,last_active,NOW()) FROM sessions WHERE id=?";
  MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql));
  MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_STRING; b.buffer=(void*)token.c_str(); b.buffer_length=token.size(); mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
  MYSQL_BIND out[2]{}; unsigned long long u; long long idle; out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&u; out[1].buffer_type=MYSQL_TYPE_LONGLONG; out[1].buffer=&idle; mysql_stmt_bind_result(s,out);
  unsigned long long id=0; if(mysql_stmt_fetch(s)==0 && idle<=300){ id=u; } mysql_stmt_close(s);
  if(id){ const char* up="UPDATE sessions SET last_active=NOW() WHERE id=?"; MYSQL_STMT* us=mysql_stmt_init(db.c); mysql_stmt_prepare(us,up,strlen(up)); MYSQL_BIND bb{}; bb.buffer_type=MYSQL_TYPE_STRING; bb.buffer=(void*)token.c_str(); bb.buffer_length=token.size(); mysql_stmt_bind_param(us,&bb); mysql_stmt_execute(us); mysql_stmt_close(us);} 
  return id;
}

int main(){ ios::sync_with_stdio(false); cout << "Content-Type: text/html\r\n\r\n"; cout << "<link rel=\"stylesheet\" href=\"style.css\">"; Db db; unsigned long long uid=current_user(db);

  mysql_query(db.c, "UPDATE auctions SET status='CLOSED' WHERE status='OPEN' AND end_time<=NOW()");

  const char* sql = R"SQL(
    SELECT a.id, a.title, a.description, a.start_price, a.end_time, a.seller_id,
           COALESCE((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.id), a.start_price) AS current
    FROM auctions a
    WHERE a.status='OPEN' AND a.end_time>NOW()
    ORDER BY a.end_time ASC
  )SQL";
  MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); mysql_stmt_execute(s);
  MYSQL_BIND out[7]{}; unsigned long long id, seller; char title[121], desc[1025]; unsigned long tlen,dlen; double start,current; char endbuf[20]; unsigned long elen;
  out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&id;
  out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=title; out[1].buffer_length=120; out[1].length=&tlen;
  out[2].buffer_type=MYSQL_TYPE_STRING; out[2].buffer=desc; out[2].buffer_length=1024; out[2].length=&dlen;
  out[3].buffer_type=MYSQL_TYPE_DOUBLE; out[3].buffer=&start;
  out[4].buffer_type=MYSQL_TYPE_STRING; out[4].buffer=endbuf; out[4].buffer_length=20; out[4].length=&elen;
  out[5].buffer_type=MYSQL_TYPE_LONGLONG; out[5].buffer=&seller;
  out[6].buffer_type=MYSQL_TYPE_DOUBLE; out[6].buffer=&current;
  mysql_stmt_bind_result(s,out);

  cout << "<h2>Open Auctions</h2>";
  cout << "<div class='small'>Ending soonest first.</div>";
  cout << "<div class='card'><ul class='list'>";
  cout.setf(std::ios::fixed); cout<<setprecision(2);
  while(mysql_stmt_fetch(s)==0){
    string t(title,tlen), d(desc,dlen), et(endbuf,elen);
    cout << "<li><span><strong>"<<t<<"</strong><br><span class='small'>"<<d<<"</span><br><span class='small'>Ends "<<et<<"</span></span><span>$"<<current;
    if(uid && uid!=seller){ cout << " <a href='trade.html'>Bid</a>"; }
    cout << "</span></li>";
  }
  cout << "</ul></div>";
  mysql_stmt_close(s);
  return 0;
}
