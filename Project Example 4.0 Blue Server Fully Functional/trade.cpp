// trade.cpp — build to public_html/trade.cgi
// g++ -O2 -std=c++17 trade.cpp -o trade.cgi -lmysqlclient
#include <bits/stdc++.h>
#include <mysql/mysql.h>
using namespace std;

const string DB_HOST="localhost", DB_USER="allindragons", DB_PASS="snogardnilla_002", DB_NAME="cs370_section2_allindragons";

string get_cookie(const string& name){ string h=getenv("HTTP_COOKIE")?getenv("HTTP_COOKIE"):""; string k=name+"="; size_t p=h.find(k); if(p==string::npos) return ""; size_t e=h.find(';',p); return h.substr(p+k.size(), (e==string::npos?h.size():e)-p-k.size()); }
string url_decode(const string &s){ string o; o.reserve(s.size()); for(size_t i=0;i<s.size();++i){ if(s[i]=='+') o+=' '; else if(s[i]=='%' && i+2<s.size()){ string hex=s.substr(i+1,2); o+= static_cast<char>(strtol(hex.c_str(), nullptr, 16)); i+=2; } else o+=s[i]; } return o; }
map<string,string> parse_post(){ string len_s = getenv("CONTENT_LENGTH")? getenv("CONTENT_LENGTH"):"0"; int len = atoi(len_s.c_str()); string body(len,'\0'); if(len>0) fread(&body[0],1,len,stdin); map<string,string> m; size_t i=0; while(i<body.size()){ size_t eq=body.find('=',i); if(eq==string::npos) break; size_t amp=body.find('&',eq); string k=url_decode(body.substr(i,eq-i)); string v=url_decode(body.substr(eq+1,(amp==string::npos? body.size():amp)-eq-1)); m[k]=v; if(amp==string::npos) break; i=amp+1; } return m; }

struct Db{ MYSQL* c=nullptr; Db(){ c=mysql_init(nullptr); mysql_real_connect(c,DB_HOST.c_str(),DB_USER.c_str(),DB_PASS.c_str(),DB_NAME.c_str(),0,nullptr,0);} ~Db(){ if(c) mysql_close(c);} };

bool get_user(Db& db, const string& token, unsigned long long& uid){ if(token.empty()) return false; const char* sql="SELECT user_id, TIMESTAMPDIFF(SECOND,last_active,NOW()) FROM sessions WHERE id=?"; MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_STRING; b.buffer=(void*)token.c_str(); b.buffer_length=token.size(); mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s); MYSQL_BIND out[2]{}; unsigned long long u; long long idle; out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&u; out[1].buffer_type=MYSQL_TYPE_LONGLONG; out[1].buffer=&idle; mysql_stmt_bind_result(s,out); bool ok=false; if(mysql_stmt_fetch(s)==0 && idle<=300){ uid=u; ok=true; } mysql_stmt_close(s); if(ok){ const char* up="UPDATE sessions SET last_active=NOW() WHERE id=?"; MYSQL_STMT* us=mysql_stmt_init(db.c); mysql_stmt_prepare(us,up,strlen(up)); MYSQL_BIND bb{}; bb.buffer_type=MYSQL_TYPE_STRING; bb.buffer=(void*)token.c_str(); bb.buffer_length=token.size(); mysql_stmt_bind_param(us,&bb); mysql_stmt_execute(us); mysql_stmt_close(us);} return ok; }

void head(){ cout << "Content-Type: text/html\r\n\r\n"; cout << "<link rel=\"stylesheet\" href=\"style.css\">"; }

int main(){ ios::sync_with_stdio(false); Db db; string token=get_cookie("session"); unsigned long long uid=0; string qs=getenv("QUERY_STRING")?getenv("QUERY_STRING"):"";

  if(qs.find("action=options")!=string::npos){
    const char* sql="SELECT id,title FROM auctions WHERE status='OPEN' AND end_time>NOW() ORDER BY end_time ASC";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql)); mysql_stmt_execute(s);
    MYSQL_BIND out[2]{}; unsigned long long id; char title[121]; unsigned long tlen;
    out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&id; out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=title; out[1].buffer_length=120; out[1].length=&tlen; mysql_stmt_bind_result(s,out);
    cout << "Content-Type: text/html\r\n\r\n";
    while(mysql_stmt_fetch(s)==0){ cout << "<option value='"<<id<<"'>"<<string(title,tlen)<<"</option>"; }
    mysql_stmt_close(s); return 0; }

  if(!get_user(db, token, uid)){ head(); cout << "<div class='notice'>Session expired (5 minutes idle). <a href='auth.html'>Sign in</a>.</div>"; return 0; }

  auto form = parse_post(); string action=form["action"];

  if(action=="sell"){
    string title=form["title"], desc=form["description"], start_price_s=form["start_price"], start_time=form["start_time"]; if(title.empty()||desc.empty()||start_price_s.empty()||start_time.empty()){ head(); cout<<"<p>Missing fields.</p>"; return 0; }
    double sp=stod(start_price_s);
    const char* sql="INSERT INTO auctions(seller_id,title,description,start_price,start_time,end_time,status) VALUES(?,?,?,?,?, DATE_ADD(?, INTERVAL 168 HOUR), 'OPEN')";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,sql,strlen(sql));
    MYSQL_BIND b[6]{};
    b[0].buffer_type=MYSQL_TYPE_LONGLONG; b[0].buffer=&uid;
    b[1].buffer_type=MYSQL_TYPE_STRING; b[1].buffer=(void*)title.c_str(); b[1].buffer_length=title.size();
    b[2].buffer_type=MYSQL_TYPE_STRING; b[2].buffer=(void*)desc.c_str();  b[2].buffer_length=desc.size();
    b[3].buffer_type=MYSQL_TYPE_DOUBLE; b[3].buffer=&sp;
    b[4].buffer_type=MYSQL_TYPE_STRING; b[4].buffer=(void*)start_time.c_str(); b[4].buffer_length=start_time.size();
    b[5].buffer_type=MYSQL_TYPE_STRING; b[5].buffer=(void*)start_time.c_str(); b[5].buffer_length=start_time.size();
    mysql_stmt_bind_param(s,b); if(mysql_stmt_execute(s)!=0){ head(); cout<<"<p>Error listing item.</p>"; } else { head(); cout<<"<p>Item listed for 7 days. <a href='listings.html'>View listings</a></p>"; }
    mysql_stmt_close(s); return 0; }

  if(action=="bid"){
    unsigned long long auction_id = strtoull(form["auction_id"].c_str(),nullptr,10); double amount = stod(form["amount"]);
    const char* chk = "SELECT seller_id, status, end_time FROM auctions WHERE id=?";
    MYSQL_STMT* s=mysql_stmt_init(db.c); mysql_stmt_prepare(s,chk,strlen(chk)); MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_LONGLONG; b.buffer=&auction_id; mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
    MYSQL_BIND out[3]{}; unsigned long long seller; char status[6]; unsigned long slen; char endbuf[20]; unsigned long elen;
    out[0].buffer_type=MYSQL_TYPE_LONGLONG; out[0].buffer=&seller; out[1].buffer_type=MYSQL_TYPE_STRING; out[1].buffer=status; out[1].buffer_length=6; out[1].length=&slen; out[2].buffer_type=MYSQL_TYPE_STRING; out[2].buffer=endbuf; out[2].buffer_length=20; out[2].length=&elen; mysql_stmt_bind_result(s,out);
    if(mysql_stmt_fetch(s)!=0){ head(); cout<<"<p>Auction not found.</p>"; mysql_stmt_close(s); return 0; }
    mysql_stmt_close(s);
    if(seller==uid){ head(); cout<<"<p>You cannot bid on your own item.</p>"; return 0; }
    const char* topsql = "SELECT GREATEST(IFNULL(MAX(amount),0), (SELECT start_price FROM auctions WHERE id=?)) FROM bids WHERE auction_id=?";
    MYSQL_STMT* ts=mysql_stmt_init(db.c); mysql_stmt_prepare(ts, topsql, strlen(topsql)); MYSQL_BIND tb[2]{}; tb[0].buffer_type=MYSQL_TYPE_LONGLONG; tb[0].buffer=&auction_id; tb[1]=tb[0]; mysql_stmt_bind_param(ts,tb); mysql_stmt_execute(ts); MYSQL_BIND tout{}; double top; tout.buffer_type=MYSQL_TYPE_DOUBLE; tout.buffer=&top; mysql_stmt_bind_result(ts,&tout); mysql_stmt_fetch(ts); mysql_stmt_close(ts);
    if(amount <= top){ head(); cout<<"<p>Bid must be greater than current top ($"<<fixed<<setprecision(2)<<top<<").</p>"; return 0; }
    const char* ins = "INSERT INTO bids(auction_id,bidder_id,amount) VALUES(?,?,?)";
    MYSQL_STMT* is=mysql_stmt_init(db.c); mysql_stmt_prepare(is,ins,strlen(ins)); MYSQL_BIND ib[3]{}; ib[0].buffer_type=MYSQL_TYPE_LONGLONG; ib[0].buffer=&auction_id; ib[1].buffer_type=MYSQL_TYPE_LONGLONG; ib[1].buffer=&uid; ib[2].buffer_type=MYSQL_TYPE_DOUBLE; ib[2].buffer=&amount; mysql_stmt_bind_param(is,ib); if(mysql_stmt_execute(is)!=0){ head(); cout<<"<p>Error placing bid.</p>"; } else { head(); cout<<"<p>Bid placed! <a href='transactions.html'>View my bids</a></p>"; } mysql_stmt_close(is);
    return 0; }

  head(); cout << "<p>Unsupported action.</p>"; return 0;
}
