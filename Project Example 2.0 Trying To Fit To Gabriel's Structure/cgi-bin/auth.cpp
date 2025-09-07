#include "auth.h"
#include "config.h"
#include "utils.h"
#include <cstring>

long current_user_and_slide(MYSQL* c){
    std::string ck = getenv_str("HTTP_COOKIE");
    auto jar = parse_cookies(ck);
    std::string token = cookie_get(jar, SESSION_COOKIE_NAME);
    if(token.empty()) return -1;

    const char* q1 = "SELECT user_id FROM sessions WHERE session_id=? AND expires_at>NOW() LIMIT 1";
    MYSQL_STMT* s1 = mysql_stmt_init(c); if(!s1) return -1;
    mysql_stmt_prepare(s1, q1, (unsigned long)strlen(q1));
    unsigned long tlen=0; MYSQL_BIND pb = bind_in_str(token, tlen);
    mysql_stmt_bind_param(s1, &pb); if(mysql_stmt_execute(s1)!=0){ mysql_stmt_close(s1); return -1; }
    long uid = fetch_one_long(s1); mysql_stmt_free_result(s1); mysql_stmt_close(s1);
    if(uid<=0) return -1;

    const char* q2 = "UPDATE sessions SET expires_at=DATE_ADD(NOW(), INTERVAL ? MINUTE) WHERE session_id=?";
    MYSQL_STMT* s2 = mysql_stmt_init(c); mysql_stmt_prepare(s2, q2, (unsigned long)strlen(q2));
    long long mins = IDLE_TIMEOUT_MINUTES; unsigned long tlen2=0; MYSQL_BIND upb[2]; std::memset(upb,0,sizeof(upb));
    upb[0]=bind_in_longlong(mins); upb[1]=bind_in_str(token,tlen2);
    mysql_stmt_bind_param(s2, upb); mysql_stmt_execute(s2); mysql_stmt_close(s2);
    return uid;
}

bool create_session(MYSQL* c, long long user_id, std::string& out_token){
    out_token = rand_hex(32);
    const char* q = "INSERT INTO sessions(session_id,user_id,expires_at) VALUES(?, ?, DATE_ADD(NOW(), INTERVAL ? MINUTE))";
    MYSQL_STMT* s = mysql_stmt_init(c); mysql_stmt_prepare(s, q, (unsigned long)strlen(q));
    long long mins = IDLE_TIMEOUT_MINUTES; unsigned long tlen=0; MYSQL_BIND pb[3]; std::memset(pb,0,sizeof(pb));
    pb[0]=bind_in_str(out_token,tlen); pb[1]=bind_in_longlong(user_id); pb[2]=bind_in_longlong(mins);
    mysql_stmt_bind_param(s, pb); bool ok = (mysql_stmt_execute(s)==0); mysql_stmt_close(s); return ok;
}

bool user_exists(MYSQL* c, const std::string& email){
    const char* q = "SELECT user_id FROM users WHERE email=? LIMIT 1";
    MYSQL_STMT* s = mysql_stmt_init(c); mysql_stmt_prepare(s,q,(unsigned long)strlen(q));
    unsigned long el=0; MYSQL_BIND b=bind_in_str(email,el); mysql_stmt_bind_param(s,&b); mysql_stmt_execute(s);
    long id = fetch_one_long(s); mysql_stmt_free_result(s); mysql_stmt_close(s); return id>0;
}

bool register_user(MYSQL* c, const std::string& email, const std::string& pass, long long& out_user_id, std::string& out_session_token){
    if(user_exists(c,email)) return false;
    std::string salt = rand_hex(16);
    const char* q = "INSERT INTO users(email,password_salt,password_hash) VALUES(?, ?, SHA2(CONCAT(?, ?),256))";
    MYSQL_STMT* s = mysql_stmt_init(c); mysql_stmt_prepare(s,q,(unsigned long)strlen(q));
    unsigned long e=0,s1=0,s2=0,p=0; MYSQL_BIND pb[4]; std::memset(pb,0,sizeof(pb));
    pb[0]=bind_in_str(email,e); pb[1]=bind_in_str(salt,s1); pb[2]=bind_in_str(salt,s2); pb[3]=bind_in_str(pass,p);
    mysql_stmt_bind_param(s,pb);
    if(mysql_stmt_execute(s)!=0){ mysql_stmt_close(s); return false; }
    out_user_id = (long long)mysql_insert_id(c); mysql_stmt_close(s);
    return create_session(c, out_user_id, out_session_token);
}

bool login_user(MYSQL* c, const std::string& email, const std::string& pass, long long& out_user_id, std::string& out_session_token){
    const char* q = "SELECT user_id FROM users WHERE email=? AND password_hash=SHA2(CONCAT(password_salt,?),256) LIMIT 1";
    MYSQL_STMT* s = mysql_stmt_init(c); mysql_stmt_prepare(s,q,(unsigned long)strlen(q));
    unsigned long e=0,p=0; MYSQL_BIND pb[2]; std::memset(pb,0,sizeof(pb)); pb[0]=bind_in_str(email,e); pb[1]=bind_in_str(pass,p);
    mysql_stmt_bind_param(s,pb); mysql_stmt_execute(s);
    long uid = fetch_one_long(s); mysql_stmt_free_result(s); mysql_stmt_close(s);
    if(uid<=0) return false; out_user_id = uid; return create_session(c, out_user_id, out_session_token);
}

std::string make_set_cookie_header(const std::string& token){
    return std::string("Set-Cookie: ") + SESSION_COOKIE_NAME + "=" + token + "; Path=/; HttpOnly; SameSite=Lax; Secure\r\n";
}
