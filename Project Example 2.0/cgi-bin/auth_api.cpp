// STEP 2: Back-end Auth API (C++ CGI) — login/register/me, prepared statements, TLS, 5-min idle timeout

#include "api_common.hpp"

int main(){
    string method = getenv("REQUEST_METHOD")?getenv("REQUEST_METHOD"):"GET";
    MYSQL* c = db(); if(!c){ http_headers(); cout<<R"({"ok":false,"error":"db"})"; return 0; }

    if(method=="GET"){
        auto kv = parse_form(getenv("QUERY_STRING")?getenv("QUERY_STRING"):"");
        if(kv.count("action") && kv["action"]=="me"){
            long uid=current_user(c);
            http_headers(); cout<<"{\"ok\":true,\"user_id\":"<<uid<<",\"idle_minutes\":"<<IDLE_TIMEOUT_MINUTES<<"}";
            mysql_close(c); return 0;
        }
        http_headers(); cout<<R"({"ok":false,"error":"unknown"})"; mysql_close(c); return 0;
    }

    // POST
    auto kv = parse_form(read_body());
    const string action=kv["action"], email=kv["email"], pass=kv["password"];
    if(email.empty()||pass.empty()||(action!="register" && action!="login")){
        http_headers(); cout<<R"({"ok":false,"error":"bad_input"})"; mysql_close(c); return 0;
    }

    if(action=="register"){
        // exists?
        const char* q1="SELECT user_id FROM users WHERE email=? LIMIT 1";
        MYSQL_STMT* s1=mysql_stmt_init(c); mysql_stmt_prepare(s1,q1,(unsigned long)strlen(q1));
        unsigned long elen=0; MYSQL_BIND b1=bind_in_str(email,elen);
        mysql_stmt_bind_param(s1,&b1); mysql_stmt_execute(s1);
        long exists=fetch_one_long(s1); mysql_stmt_free_result(s1); mysql_stmt_close(s1);
        if(exists>0){ http_headers(); cout<<R"({"ok":false,"error":"exists"})"; mysql_close(c); return 0; }

        // insert user
        string salt=rand_hex(16);
        const char* q2="INSERT INTO users(email,password_salt,password_hash) VALUES(?, ?, SHA2(CONCAT(?, ?),256))";
        MYSQL_STMT* s2=mysql_stmt_init(c); mysql_stmt_prepare(s2,q2,(unsigned long)strlen(q2));
        unsigned long sl1=0,sl2=0,pl=0; MYSQL_BIND pb2[4]; memset(pb2,0,sizeof(pb2));
        pb2[0]=bind_in_str(email,elen); pb2[1]=bind_in_str(salt,sl1); pb2[2]=bind_in_str(salt,sl2); pb2[3]=bind_in_str(pass,pl);
        mysql_stmt_bind_param(s2,pb2);
        if(mysql_stmt_execute(s2)!=0){ mysql_stmt_close(s2); http_headers(); cout<<R"({"ok":false,"error":"insert_user"})"; mysql_close(c); return 0; }
        my_ulonglong uid=mysql_insert_id(c); mysql_stmt_close(s2);

        // session with 5-minute idle timeout
        string tok=rand_hex(32);
        const char* q3="INSERT INTO sessions(session_id,user_id,expires_at) VALUES(?, ?, DATE_ADD(NOW(), INTERVAL ? MINUTE))";
        MYSQL_STMT* s3=mysql_stmt_init(c); mysql_stmt_prepare(s3,q3,(unsigned long)strlen(q3));
        long long uidll=(long long)uid, mins=IDLE_TIMEOUT_MINUTES; unsigned long tl=0;
        MYSQL_BIND pb3[3]; memset(pb3,0,sizeof(pb3));
        pb3[0]=bind_in_str(tok,tl); pb3[1]=bind_in_longlong(uidll); pb3[2]=bind_in_longlong(mins);
        mysql_stmt_bind_param(s3,pb3);
        if(mysql_stmt_execute(s3)!=0){ mysql_stmt_close(s3); http_headers(); cout<<R"({"ok":false,"error":"session"})"; mysql_close(c); return 0; }
        mysql_stmt_close(s3);

        http_headers("application/json","Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax; Secure\r\n");
        cout<<"{\"ok\":true,\"user_id\":"<<uid<<"}";
        mysql_close(c); return 0;
    }

    if(action=="login"){
        const char* q="SELECT user_id FROM users WHERE email=? AND password_hash=SHA2(CONCAT(password_salt,?),256) LIMIT 1";
        MYSQL_STMT* s=mysql_stmt_init(c); mysql_stmt_prepare(s,q,(unsigned long)strlen(q));
        unsigned long el=0,pl=0; MYSQL_BIND pb[2]; memset(pb,0,sizeof(pb)); pb[0]=bind_in_str(email,el); pb[1]=bind_in_str(pass,pl);
        mysql_stmt_bind_param(s,pb); mysql_stmt_execute(s);
        long uid=fetch_one_long(s); mysql_stmt_free_result(s); mysql_stmt_close(s);
        if(uid<=0){ http_headers(); cout<<R"({"ok":false,"error":"invalid"})"; mysql_close(c); return 0; }

        string tok=rand_hex(32);
        const char* q2="INSERT INTO sessions(session_id,user_id,expires_at) VALUES(?, ?, DATE_ADD(NOW(), INTERVAL ? MINUTE))";
        MYSQL_STMT* s2=mysql_stmt_init(c); mysql_stmt_prepare(s2,q2,(unsigned long)strlen(q2));
        long long uidll=uid, mins=IDLE_TIMEOUT_MINUTES; unsigned long tl=0; MYSQL_BIND pb2[3]; memset(pb2,0,sizeof(pb2));
        pb2[0]=bind_in_str(tok,tl); pb2[1]=bind_in_longlong(uidll); pb2[2]=bind_in_longlong(mins);
        mysql_stmt_bind_param(s2,pb2);
        if(mysql_stmt_execute(s2)!=0){ mysql_stmt_close(s2); http_headers(); cout<<R"({"ok":false,"error":"session"})"; mysql_close(c); return 0; }
        mysql_stmt_close(s2);

        http_headers("application/json","Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax; Secure\r\n");
        cout<<"{\"ok\":true,\"user_id\":"<<uid<<"}";
        mysql_close(c); return 0;
    }

    http_headers(); cout<<R"({"ok":false,"error":"unknown"})"; mysql_close(c); return 0;
}