#include "../include/cgi_utils.hpp"
#include <openssl/sha.h>

using namespace std;
using namespace mini;

static string sha256(const string& s){
  unsigned char h[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char*)s.data(), s.size(), h);
  return string((char*)h, SHA256_DIGEST_LENGTH);
}
static string random_bytes(size_t n){
  std::ifstream ur("/dev/urandom", ios::binary);
  string s(n,'\0'); ur.read(&s[0], n); return s;
}
static string hex(const string& bin){
  static const char* d="0123456789abcdef";
  string o; o.reserve(bin.size()*2);
  for(unsigned char c: bin){ o.push_back(d[c>>4]); o.push_back(d[c&15]); }
  return o;
}

int main(){
  try{
    CGI c; c.parse();
    DB db;

    string action = c.params.count("action")?c.params["action"]:"";
    if (action=="logout"){
      string sid = get_cookie(c.cookie, "ASESSION");
      if(!sid.empty()) db.exec("DELETE FROM sessions WHERE id='"+db.esc(sid)+"'");
      hdr_ok("Set-Cookie: ASESSION=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
      page("Logged out","<h1>Logged out</h1><p><a href='/index.html'>Home</a></p>");
      return 0;
    }

    if (c.method=="POST" && (action=="register" || action=="login")){
      string email = c.params["email"], pass = c.params["password"];
      if(email.empty() || pass.empty()){
        hdr_ok(); page("Error","<h1>Missing fields</h1><p><a href='/auth.cgi'>Back</a></p>"); return 0;
      }
      if(action=="register"){
        string salt = random_bytes(32);
        string ph = sha256(salt + pass);
        string q = "INSERT INTO users(email,password_hash,password_salt) VALUES('"+db.esc(email)+"',"
                   "0x"+hex(ph)+",0x"+hex(salt)+")";
        if(mysql_query(db.conn, q.c_str())){
          hdr_ok(); page("Error","<h1>Registration failed (email in use?)</h1><p><a href='/auth.cgi'>Back</a></p>"); return 0;
        }
      }
      string q = "SELECT id, HEX(password_hash), HEX(password_salt) FROM users WHERE email='"+db.esc(email)+"'";
      db.exec(q);
      MYSQL_RES* r = mysql_store_result(db.conn); MYSQL_ROW row = mysql_fetch_row(r);
      if(!row){ mysql_free_result(r); hdr_ok(); page("Error","<h1>Invalid credentials</h1><p><a href='/auth.cgi'>Back</a></p>"); return 0; }
      int uid = atoi(row[0]);
      auto hex2bin=[&](const string& hx){ string o; o.reserve(hx.size()/2);
        for(size_t i=0;i+1<hx.size(); i+=2){ unsigned v; sscanf(hx.substr(i,2).c_str(), "%02x",&v); o.push_back((char)v); } return o; };
      string p_hash = hex2bin(row[1]); string p_salt = hex2bin(row[2]); mysql_free_result(r);
      if(sha256(p_salt + pass) != p_hash){
        hdr_ok(); page("Error","<h1>Invalid credentials</h1><p><a href='/auth.cgi'>Back</a></p>"); return 0;
      }
      string sid = hex(random_bytes(32));
      string ins = "INSERT INTO sessions(id,user_id,last_activity) VALUES('"+db.esc(sid)+"',"+to_string(uid)+","+now_sql_utc()+")";
      db.exec(ins);
      hdr_ok("Set-Cookie: ASESSION="+sid+"; Path=/; HttpOnly; SameSite=Lax");
      page("Welcome","<h1>Welcome</h1><p><a href='/listings.cgi'>View Listings</a> · <a href='/trade.cgi'>Bid/Sell</a> · <a href='/transactions.cgi'>My Transactions</a></p>");
      return 0;
    }

    optional<int> uid = require_session(db, c, true);
    string notice = "";
    if(!uid.has_value() && !get_cookie(c.cookie,"ASESSION").empty()){
      notice = "<p class='warn'>Session expired due to 5 minutes of inactivity. Please log in again.</p>";
    }

    hdr_ok();

    // Build the HTML body in a std::string, then conditionally append the logout link.
    std::string body =
      notice +
      std::string(
        "<h1>Login</h1>"
        "<form method='post' action='/auth.cgi'><input type='hidden' name='action' value='login'>"
        "<label>Email <input name='email' type='email' required></label>"
        "<label>Password <input name='password' type='password' required></label>"
        "<button>Log in</button></form>"
        "<h2>Register</h2>"
        "<form method='post' action='/auth.cgi'><input type='hidden' name='action' value='register'>"
        "<label>Email <input name='email' type='email' required></label>"
        "<label>Password <input name='password' type='password' required></label>"
        "<button>Create account</button></form>"
      );

    if (uid) {
      body += "<p><a href='/auth.cgi?action=logout'>Logout</a></p>";
    }

    page("Login/Register", body);

  } catch(const exception& e){
    cout << "Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n" << e.what();
  }
  return 0;
}