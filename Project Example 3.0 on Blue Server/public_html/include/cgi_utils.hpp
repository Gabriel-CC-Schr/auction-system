
#ifndef CGI_UTILS_HPP
#define CGI_UTILS_HPP

#include <bits/stdc++.h>
#include <mysql.h>

namespace mini {

struct CGI {
  std::string method, query, body, cookie;
  std::map<std::string,std::string> params;
  CGI() {
    const char* m = getenv("REQUEST_METHOD"); method = m?m:"GET";
    const char* q = getenv("QUERY_STRING");  query  = q?q:"";
    const char* ck= getenv("HTTP_COOKIE");   cookie = ck?ck:"";
    if (method=="POST") {
      int len = atoi(getenv("CONTENT_LENGTH")?getenv("CONTENT_LENGTH"):"0");
      body.resize(len);
      if (len) fread(&body[0],1,len,stdin);
    }
  }
  static std::string url_decode(const std::string& s){
    std::string o; o.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
      if(s[i]=='+') o.push_back(' ');
      else if(s[i]=='%' && i+2<s.size()){
        int v; sscanf(s.substr(i+1,2).c_str(), "%x", &v);
        o.push_back(static_cast<char>(v)); i+=2;
      } else o.push_back(s[i]);
    } return o;
  }
  static std::map<std::string,std::string> parse_kv(const std::string& s){
    std::map<std::string,std::string> m;
    std::stringstream ss(s); std::string kv;
    while(std::getline(ss, kv, '&')){
      auto p = kv.find('=');
      std::string k = url_decode(p==std::string::npos?kv:kv.substr(0,p));
      std::string v = url_decode(p==std::string::npos?"":kv.substr(p+1));
      m[k]=v;
    } return m;
  }
  void parse(){
    params = parse_kv(query);
    if(method=="POST"){
      auto pp = parse_kv(body);
      params.insert(pp.begin(), pp.end());
    }
  }
};

struct DB {
  MYSQL* conn;
  DB() {
    conn = mysql_init(NULL);
    const char *h=getenv("DB_HOST"), *u=getenv("DB_USER"), *p=getenv("DB_PASS"), *d=getenv("DB_NAME");
    if(!mysql_real_connect(conn, h?h:"localhost", u?u:"root", p?p:"", d?d:"auction_db", 0, NULL, 0)){
      throw std::runtime_error(mysql_error(conn));
    }
    mysql_set_character_set(conn,"utf8mb4");
  }
  ~DB(){ if(conn) mysql_close(conn); }
  void exec(const std::string& q){ if(mysql_query(conn, q.c_str())) throw std::runtime_error(mysql_error(conn)); }
  std::string esc(const std::string& s){
    std::string out; out.resize(s.size()*2+1);
    unsigned long l = mysql_real_escape_string(conn,&out[0],s.c_str(),s.size());
    out.resize(l); return out;
  }
};

inline std::string get_cookie(const std::string& hdr, const std::string& key){
  std::string p = key + "=";
  std::stringstream ss(hdr); std::string part;
  while(std::getline(ss, part, ';')){
    while(!part.empty() && part.front()==' ') part.erase(part.begin());
    if(part.rfind(p,0)==0) return part.substr(p.size());
  } return "";
}

inline std::string now_sql_utc(){ return "UTC_TIMESTAMP()"; }

inline std::optional<int> require_session(DB& db, const CGI& cgi, bool refresh=true){
  std::string sid = get_cookie(cgi.cookie,"ASESSION");
  if(sid.empty()) return std::nullopt;
  db.exec("SELECT user_id, UNIX_TIMESTAMP(UTC_TIMESTAMP())-UNIX_TIMESTAMP(last_activity) "
          "FROM sessions WHERE id='"+db.esc(sid)+"'");
  MYSQL_RES* r = mysql_store_result(db.conn); MYSQL_ROW row = mysql_fetch_row(r);
  if(!row){ mysql_free_result(r); return std::nullopt; }
  int uid = atoi(row[0]); long idle = atol(row[1]); mysql_free_result(r);
  if(idle>300){ db.exec("DELETE FROM sessions WHERE id='"+db.esc(sid)+"'"); return std::nullopt; }
  if(refresh) db.exec("UPDATE sessions SET last_activity="+now_sql_utc()+" WHERE id='"+db.esc(sid)+"'");
  return uid;
}

inline void hdr_ok(const std::string& extra=""){
  std::cout << "Content-Type: text/html; charset=utf-8\r\n" << extra << "\r\n";
}
inline void page(const std::string& title, const std::string& body){
  std::cout << "<!doctype html><html><head><meta charset='utf-8'><title>"
            << title << "</title><link rel='stylesheet' href='/style.css'></head><body><div class='wrap'>"
            << body << "</div></body></html>";
}

} // namespace mini
#endif
